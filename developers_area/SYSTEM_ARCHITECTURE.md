# System Architecture

## Overview

The Duplicate File Finder is built with a modular, three-component architecture designed for flexibility and scalability.

---

## Component Diagram

```
┌─ Standalone Scanner ─────────────────────────┐
│ ./duplicate_finder /path                      │
│ - One-time scan                               │
│ - Outputs results to terminal                 │
└───────────────────────────────────────────────┘

┌─ Background Service ──────────────────────────┐
│ Daemon (duplicate-finder-daemon)              │
│ ├─ Periodic scanning (every 1 hour)          │
│ ├─ Unix socket server                        │
│ ├─ Result caching                            │
│ └─ Log to syslog                             │
└───────────────────────────────────────────────┘

┌─ CLI Client ──────────────────────────────────┐
│ duplicate-finder-client <command>             │
│ ├─ status   - Check daemon status            │
│ ├─ results  - Get latest results             │
│ ├─ scan     - Trigger immediate scan         │
│ └─ help     - Show help                      │
└───────────────────────────────────────────────┘
```

---

## Component Details

### 1. Standalone Scanner (`duplicate_finder`)

**Purpose**: Direct, one-time filesystem scanning

**Responsibilities**:
- Recursive directory traversal
- File metadata collection
- Multi-threaded SHA256 hashing
- Duplicate detection via two-phase algorithm
- Result display to terminal

**Usage**:
```bash
./duplicate_finder /home
./duplicate_finder /home /var /opt
```

**Advantages**:
- No dependencies on daemon
- Immediate results
- Easy to use for quick scans
- Good for scripting/automation

---

### 2. Background Daemon (`duplicate-finder-daemon`)

**Purpose**: Continuous background scanning with persistent result caching

**Responsibilities**:
- Daemonization and process management
- Periodic scanning at configured intervals
- Result persistence to disk
- Signal handling (SIGTERM, SIGINT, SIGHUP)
- Unix domain socket server for IPC
- Logging to syslog

**Files Created**:
- `/var/run/duplicate-finder.pid` - Process ID file
- `/tmp/duplicate-finder.sock` - Unix socket for communication
- `/var/cache/duplicate-finder/results.db` - Cached results

**Features**:
- Runs in background after boot
- Automatic periodic scanning (default: 1 hour)
- Can be manually triggered by client
- Proper cleanup and signal handling
- syslog integration for monitoring

**Configuration**:
- Scan interval: Edit `SCAN_INTERVAL` in daemon.c
- Directories: Edit scan paths in `perform_scan()`
- Resource limits: Configure in systemd service file

---

### 3. CLI Client (`duplicate-finder-client`)

**Purpose**: User interface to query daemon and request operations

**Responsibilities**:
- Socket communication with daemon
- User command parsing
- Result display formatting
- Error reporting

**Commands**:
- `status` - Check if daemon is running
- `results` - Retrieve latest scan results
- `scan` - Request immediate scan
- `help` - Show command help

**Communication Protocol**:
- Unix domain socket connection
- Text-based command/response protocol
- Blocks until response received or error

---

## Data Flow

### Standalone Scanner Flow

```
User → ./duplicate_finder /path
  ↓
Scan directory recursively
  ↓
Group files by size
  ↓
Calculate hashes for groups
  ↓
Group by hash
  ↓
Display duplicates
  ↓
Exit
```

### Daemon/Client Flow

```
Daemon starts → Setup socket → Listen for connections
                      ↑
                      │
User → duplicate-finder-client status
                      │
                      ↓
Client connects to socket → Send command
                             ← Receive response
                      ↓
Display results
```

---

## IPC (Inter-Process Communication)

### Socket Details

- **Type**: Unix Domain Socket (AF_UNIX)
- **Path**: `/tmp/duplicate-finder.sock`
- **Protocol**: Text-based command/response
- **Permissions**: 0777 (world accessible)

### Message Format

**Client → Daemon**:
```
GET_RESULTS     - Request cached results
START_SCAN      - Request immediate scan
STATUS          - Request daemon status
```

**Daemon → Client**:
```
Results data (multiline)
SCAN_STARTED
DAEMON_RUNNING|PID:1234|TIME:1234567890
```

---

## Multi-Threading Architecture

### Hashing Threads

**Design**:
- One thread per CPU core (configurable)
- Each thread hashes a portion of files
- Synchronized access to shared data structures

**Thread Pool**:
- Master thread organizes work
- Worker threads process hashes
- Barrier synchronization for completion

**Benefits**:
- Linear speedup with cores (4 cores ≈ 4x faster)
- Efficient resource utilization
- Scales with hardware capabilities

---

## Memory Management

### Data Structures

```c
FileEntry {
    path[MAX_PATH]          // File path
    size                    // File size
    mtime                   // Modification time
    hash[SHA256_DIGEST]     // SHA256 hash
    hash_calculated         // Flag
}

DuplicateSet {
    hash[SHA256_DIGEST]     // Hash of files
    size                    // File size
    files[]                 // Array of FileEntry pointers
    file_count              // Number of files
}

FileDatabase {
    files[]                 // All scanned files
    file_count              // Total files
    duplicates[]            // Duplicate sets found
    duplicate_count         // Total sets
}
```

### Memory Limits

- `MAX_FILES`: 100,000 files
- Adjustable based on available RAM
- Fallback error handling if limit exceeded

---

## Synchronization & Safety

### Process Safety

- Second fork ensures no terminal access
- Proper signal handlers prevent orphaned processes
- PID file prevents multiple daemon instances
- Resource cleanup on exit

### File Safety

- Read-only file access
- No file modifications
- Respects file permissions
- Handles permission denied gracefully

### Data Consistency

- Mutex locks for shared database
- Atomic operations where needed
- Proper cleanup of allocated memory
- Safe socket operations

---

## Performance Optimization

### Size-Based Pre-filtering

```
All files → Group by size → Skip unique sizes → Hash only groups
                                                 (80% eliminated)
```

### Caching Strategy

- Daemon caches results between scans
- Client queries cache first
- Can request fresh scan if needed
- Reduces redundant scanning

### Hash Computation

- Streaming SHA256 (not loading entire file)
- 4KB buffer for efficient I/O
- Multi-threaded parallel processing
- File size sorting before hashing

---

## Error Handling

### Build Time Errors
- Missing dependencies detected by Makefile
- Clear error messages for compilation failures

### Runtime Errors
- File permission denied → skip file
- Socket connection failed → user-friendly message
- Out of memory → error and exit
- Signal interruption → graceful shutdown

### Daemon Errors
- Logged to syslog
- Check with: `journalctl -u duplicate-finder.service`
- Service auto-restarts (if configured)

---

## Extensibility

### Adding Features

1. **Database Backend**
   - Replace flat file caching with SQLite
   - Add incremental scanning
   - Query results efficiently

2. **GUI Interface**
   - Use GTK+, Qt, or web framework
   - Connect via socket protocol
   - Same daemon backend

3. **Deduplication**
   - Create hardlinks
   - Create symlinks
   - Integrate with delete functionality

4. **Advanced Filtering**
   - File type filtering
   - Date range filtering
   - Size range filtering

---

## Security Considerations

### Running as Service

- Service file runs as root (configurable)
- Can be limited with systemd security settings
- Respects file permissions entirely
- No privilege escalation

### Socket Security

- Unix domain socket (file-based)
- Permissions enforced by filesystem
- Not exposed to network
- Only accessible locally

### File Operations

- Read-only access to files
- No file modifications
- No content storage (only hashes)
- Secure hash computation

---

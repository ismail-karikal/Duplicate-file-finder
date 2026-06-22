# Duplicate File Finder - Implementation Guide

## Project Overview

This is a complete C-based solution for finding duplicate files and folders. It consists of:

1. **duplicate_finder.c** - Main scanning engine
2. **daemon.c** - Background service that runs continuously
3. **client.c** - CLI to communicate with the daemon
4. **build.sh** - Build script

---

## System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    USER INTERFACE (CLI)                      │
│                    (client executable)                       │
└────────────────────────┬────────────────────────────────────┘
                         │ requests results
                         │ starts scans
                         ↓
┌─────────────────────────────────────────────────────────────┐
│                DAEMON (Background Service)                   │
│  - Listens on Unix socket: /tmp/duplicate-finder.sock       │
│  - Periodic scanning (configurable interval)                │
│  - Stores results in: /var/cache/duplicate-finder/          │
│  - Runs as system service                                   │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ↓
    ┌──────────────────────────────────────┐
    │  File System Scanning Module         │
    │  - Directory traversal               │
    │  - File discovery                    │
    │  - Multi-threaded hashing            │
    │  - Duplicate detection               │
    └──────────────────────────────────────┘
```

---

## Prerequisites & Dependencies

### Required Software
```bash
# Ubuntu/Debian
sudo apt-get install -y \
    build-essential          # gcc, make, libc dev
    libssl-dev               # OpenSSL development files
    git

# Fedora/RHEL
sudo dnf install -y \
    gcc                      # C compiler
    openssl-devel            # OpenSSL development files

# macOS
brew install openssl
```

### Check installations
```bash
gcc --version
pkg-config --cflags --libs openssl
```

---

## Building the Project

### Method 1: Using Build Script (Recommended)
```bash
# Make script executable
chmod +x build.sh

# Run build
./build.sh

# Result: Creates three executables
# - duplicate_finder      (main scanner)
# - duplicate-finder-daemon  (background service)
# - duplicate-finder-client  (user interface)
```

### Method 2: Manual Compilation
```bash
# Build main scanner
gcc -o duplicate_finder duplicate_finder.c \
    -lpthread $(pkg-config --cflags --libs openssl) \
    -Wall -Wextra -O2

# Build daemon
gcc -o duplicate-finder-daemon daemon.c \
    -lpthread $(pkg-config --cflags --libs openssl) \
    -Wall -Wextra -O2

# Build client
gcc -o duplicate-finder-client client.c \
    -Wall -Wextra -O2
```

---

## Usage Guide

### 1. Running as One-Time Scanner

```bash
# Scan single directory
./duplicate_finder /home

# Scan multiple directories
./duplicate_finder /home /var /opt

# Scan entire system (requires root)
sudo ./duplicate_finder /

# Output example:
# Scanning: /home
# Scanned 5000 files...
# Total files found: 5234
# Calculating hashes for 5234 files...
# Hash calculation complete!
# 
# Found 15 duplicate sets
# 
# ========== DUPLICATE FILES FOUND ==========
# 
# Duplicate Set #1:
#   Size: 1048576 bytes
#   Count: 3 files
#   Hash: a1b2c3d4...
#   Files:
#     [1] /home/user/photo1.jpg
#     [2] /home/user/photo2.jpg
#     [3] /home/backup/photo.jpg
#   Wasted space: 2097152 bytes
```

### 2. Running as Background Daemon

#### Start the daemon
```bash
# Start daemon in background
./duplicate-finder-daemon

# Check if running
./duplicate-finder-daemon --status

# Output: Daemon is running (PID: 12345)
```

#### Configure daemon paths
Edit `daemon.c` to configure:
- `#define SCAN_INTERVAL 3600` - Change scanning frequency (in seconds)
- Directory paths to scan
- Database file location

#### Stop the daemon
```bash
./duplicate-finder-daemon --stop
```

#### Run daemon in foreground (for debugging)
```bash
./duplicate-finder-daemon --foreground
```

### 3. Using the Client

```bash
# Get daemon status
./duplicate-finder-client status

# Get latest scan results
./duplicate-finder-client results

# Request immediate scan
./duplicate-finder-client scan

# Show help
./duplicate-finder-client help
```

---

## Setting Up as System Service

### Create systemd service file

Create `/etc/systemd/system/duplicate-finder.service`:

```ini
[Unit]
Description=Duplicate File Finder Daemon
After=network.target

[Service]
Type=simple
User=root
ExecStart=/usr/local/bin/duplicate-finder-daemon
Restart=on-failure
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

### Install and enable service

```bash
# Copy binaries to system path
sudo cp duplicate-finder-daemon /usr/local/bin/
sudo cp duplicate-finder-client /usr/local/bin/

# Make executable
sudo chmod +x /usr/local/bin/duplicate-finder-*

# Enable and start service
sudo systemctl enable duplicate-finder.service
sudo systemctl start duplicate-finder.service

# Check status
sudo systemctl status duplicate-finder.service

# View logs
sudo journalctl -u duplicate-finder.service -f
```

---

## Configuration

### Main Configuration Parameters

**In duplicate_finder.c:**
```c
#define MAX_FILES 100000        // Maximum files to scan
#define MAX_PATH 4096           // Maximum path length
#define BUFFER_SIZE 4096        // Read buffer for hashing
```

**In daemon.c:**
```c
#define SCAN_INTERVAL 3600      // Scan interval in seconds (1 hour)
#define DB_FILE "/var/cache/duplicate-finder/results.db"
#define SOCKET_FILE "/tmp/duplicate-finder.sock"
#define PID_FILE "/var/run/duplicate-finder.pid"
```

### Performance Tuning

**Number of threads for hashing:**
```c
int num_threads = 4;  // In hash_worker() function
```
- Increase for SSD/fast storage
- Decrease for slow disks or high load

**Scan interval:**
```c
#define SCAN_INTERVAL 3600  // Change to desired seconds
```
- 3600 = 1 hour (default)
- 1800 = 30 minutes
- 86400 = 1 day

---

## File Organization

### Directory structure after setup

```
/etc/duplicate-finder/
  └── config.conf         # Configuration file (create as needed)

/var/cache/duplicate-finder/
  ├── results.db          # Scan results database
  └── results.txt         # Human-readable results

/var/run/duplicate-finder/
  └── daemon.pid          # Process ID file

/usr/local/bin/
  ├── duplicate-finder            # Scanner executable
  ├── duplicate-finder-daemon     # Daemon executable
  └── duplicate-finder-client     # Client executable

/tmp/
  └── duplicate-finder.sock       # IPC socket
```

---

## How It Works

### Scanner Algorithm

1. **File Discovery**
   - Recursive directory traversal
   - Collect file metadata (path, size, mtime)
   - Skip hidden files and symlinks

2. **Size Grouping** (Phase 1)
   - Sort all files by size
   - Group files with identical sizes
   - Skip unique sizes (no duplicates possible)

3. **Hashing** (Phase 2)
   - Calculate SHA256 hash for each file
   - Multi-threaded for performance
   - Only process files in groups of 2+

4. **Duplicate Detection** (Phase 3)
   - Compare hashes within same-size groups
   - Files with identical size + hash = duplicates
   - Store duplicate sets with metadata

5. **Results Storage**
   - Save to database
   - Generate human-readable report
   - Calculate wasted space

### Communication Protocol

**Socket-based communication between daemon and client:**

```
Client Request          Daemon Response
─────────────────       ─────────────────
"STATUS"        →       "DAEMON_RUNNING|PID:XXX|TIME:YYY"
"GET_RESULTS"   →       [file contents]
"START_SCAN"    →       "SCAN_STARTED"
```

---

## Troubleshooting

### Build Errors

**Error: "openssl/sha.h: No such file"**
```bash
# Solution: Install OpenSSL dev
sudo apt-get install libssl-dev
# or
sudo dnf install openssl-devel
```

**Error: "undefined reference to pthread_create"**
```bash
# Solution: Link pthread library
# Make sure -lpthread is in gcc command
```

### Runtime Issues

**Daemon won't start**
```bash
# Check if already running
pgrep -a duplicate-finder

# Check syslog
sudo tail -f /var/log/syslog | grep duplicate

# Run in foreground for debugging
./duplicate-finder-daemon --foreground
```

**Client can't connect to daemon**
```bash
# Check socket file exists
ls -la /tmp/duplicate-finder.sock

# Check daemon is running
./duplicate-finder-daemon --status

# Verify permissions
stat /tmp/duplicate-finder.sock
```

**Scanner skipping files**
- Check file permissions
- Verify path is readable
- Check MAX_FILES limit

---

## Advanced Features

### Extending the Scanner

1. **Filter by file type**
```c
// Add in scan_directory()
if (strstr(full_path, ".pdf")) skip;
```

2. **Filter by date**
```c
if (entry->mtime < cutoff_time) skip;
```

3. **Custom duplicate detection**
- Implement content comparison
- Variable-length hashing
- Partial match detection

4. **Database backend**
- Integrate SQLite for queries
- Add filtering capabilities
- Historical scan tracking

### Creating a GUI

Consider using:
- **GTK+ 3** - Linux native GUI
- **Qt** - Cross-platform
- **Electron** - Web-based UI

---

## Security Considerations

1. **File Permissions**
   - Respect user permissions
   - Don't scan restricted files
   - Log access attempts

2. **Resource Limits**
   - Use setrlimit() to prevent DoS
   - Limit scan time
   - Memory usage caps

3. **Data Privacy**
   - Don't store file contents
   - Secure hash storage
   - Secure deletion of temp files

---

## Next Steps

1. ✓ Understand the architecture
2. ✓ Build the project
3. ✓ Test with sample directories
4. ✓ Set up as system service
5. ✓ Create GUI (optional)
6. ✓ Add configuration file support
7. ✓ Implement incremental scanning
8. ✓ Add logging system

---

## Development Notes

### Compilation Flags Explained

```bash
-lpthread           # Link pthread library
-lssl -lcrypto      # Link OpenSSL
-Wall -Wextra       # All warnings
-O2                 # Optimization level 2
-g                  # Debug symbols (add for debugging)
```

### Code Quality

```bash
# Check for memory leaks
valgrind --leak-check=full ./duplicate_finder /home

# Profile performance
perf record ./duplicate_finder /large/directory
perf report

# Static analysis
cppcheck duplicate_finder.c
```

---

## References

- [SHA256 in OpenSSL](https://www.openssl.org/docs/man1.1.1/man3/SHA256_Init.html)
- [POSIX Threads](https://pubs.openxmlformats.org/wordprocessingml/conformance/2016/docs/examples/)
- [Unix Domain Sockets](https://man7.org/linux/man-pages/man7/unix.7.html)
- [systemd Service Files](https://www.freedesktop.org/software/systemd/man/systemd.service.html)

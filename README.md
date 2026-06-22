# Duplicate File Finder - C Application

A complete, production-ready C application that finds and reports duplicate files and folders across your filesystem with a background daemon service and efficient multi-threaded scanning engine.

---

## 🚀 Installation & Setup

### Step 1: Install Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt-get install -y build-essential libssl-dev git
```

**Fedora/RHEL:**
```bash
sudo dnf install -y gcc openssl-devel
```

**macOS:**
```bash
brew install openssl
```

### Step 2: Clone/Download the Repository

```bash
git clone <repository-url>
cd Duplicate-file-finder
```

### Step 3: Build the Application

**Option A: Using Makefile (Recommended)**
```bash
make clean
make
```

**Option B: Using Build Script**
```bash
chmod +x build.sh
./build.sh
```

After successful build, you'll have three executables:
- `./duplicate_finder` - Standalone scanner
- `./duplicate-finder-daemon` - Background service
- `./duplicate-finder-client` - CLI client

### Step 4: Test the Installation

Test the scanner with a quick scan:
```bash
./duplicate_finder /home
```

You should see output showing:
- Number of files scanned
- Duplicate sets found
- File paths with hashes
- Wasted disk space calculation

---

## 🔧 Getting It Working - Two Options

### Option A: Run as Standalone Scanner (Simple)

```bash
# Scan a single directory
./duplicate_finder /home

# Scan multiple directories
./duplicate_finder /home /var /opt

# Scan entire system (requires root)
sudo ./duplicate_finder /
```

The scanner will display all duplicates with detailed information and exit.

### Option B: Install as System Service (Recommended)

Install and enable the daemon for continuous background scanning:

```bash
# 1. Install the service
sudo make install-service

# 2. Verify it's running
systemctl status duplicate-finder.service

# 3. Check the logs
journalctl -u duplicate-finder.service -f

# 4. Use the client to interact with the daemon
duplicate-finder-client status      # Check daemon status
duplicate-finder-client results     # View latest scan results
duplicate-finder-client scan        # Trigger immediate scan
```

The daemon will:
- Run in the background
- Automatically scan at regular intervals (default: every 1 hour)
- Cache results for quick access
- Communicate via the client tool

### To Stop or Disable the Service

```bash
# Stop the daemon
sudo systemctl stop duplicate-finder.service

# Disable autostart
sudo systemctl disable duplicate-finder.service

# Or remove completely
sudo make uninstall-service
```

---

## 📦 What's Included

### Core Application Files
- **duplicate_finder.c** - Main scanning engine with multi-threaded SHA256 hashing
- **daemon.c** - Background service with daemonization and Unix socket server
- **client.c** - CLI interface for user interaction
- **Makefile** - Automated compilation and installation
- **build.sh** - Shell script alternative build method
- **duplicate-finder.service** - systemd service definition

---

## � Algorithm Overview

### Two-Pass Duplicate Detection

The algorithm uses a two-phase approach for efficiency and accuracy:

#### Phase 1: Size-Based Grouping
- Recursively scan the filesystem
- Collect file metadata (path, size, modification time)
- Sort files by size
- Group files with identical sizes
- Skip unique sizes (no duplicates possible)

#### Phase 2: Content-Based Hashing
- For each size group with 2+ files, calculate SHA256 hash
- Group files by hash within their size group
- Files with same size + same hash = duplicates



## � For Developers

For detailed technical information about the system architecture, components, and design patterns, see the **[developers_area](developers_area/)** folder which contains:

- **[SYSTEM_ARCHITECTURE.md](developers_area/SYSTEM_ARCHITECTURE.md)** - Complete architectural overview, component details, data flow, IPC protocol, and extensibility
- **[IMPLEMENTATION_GUIDE.md](developers_area/IMPLEMENTATION_GUIDE.md)** - Build, setup, configuration, and advanced customization
- **[QUICK_REFERENCE.md](developers_area/QUICK_REFERENCE.md)** - Quick commands and debugging reference

---
# Quick Reference Guide - Duplicate File Finder

## 🔨 Build Commands

```bash
# Build all
make clean && make

# Install as system service
sudo make install-service

# Uninstall
sudo make uninstall

# Clean build artifacts
make clean

# View help
make help
```

---

## 🚀 Running the Scanner

### One-Time Scans

```bash
# Scan home directory
./duplicate_finder ~

# Scan multiple directories
./duplicate_finder /home /var /opt

# Scan entire system (requires root)
sudo ./duplicate_finder /

# Scan with progress indicator (redirected output)
./duplicate_finder /home 2>&1 | tee scan_results.txt
```

---

## 👻 Daemon Management

### Start/Stop

```bash
# Start daemon
sudo systemctl start duplicate-finder.service

# Stop daemon
sudo systemctl stop duplicate-finder.service

# Check status
sudo systemctl status duplicate-finder.service

# View logs
sudo journalctl -u duplicate-finder.service -f

# View last 50 lines of logs
sudo journalctl -u duplicate-finder.service -n 50

# Enable on boot
sudo systemctl enable duplicate-finder.service

# Disable on boot
sudo systemctl disable duplicate-finder.service

# Restart daemon
sudo systemctl restart duplicate-finder.service
```

---

## 📊 Client Commands

### Query Daemon

```bash
# Check if daemon is running
duplicate-finder-client status

# Get latest scan results
duplicate-finder-client results

# Manually trigger a scan
duplicate-finder-client scan

# Show help
duplicate-finder-client help

# Export results to file
duplicate-finder-client results > duplicates.txt

# Monitor results live
watch -n 60 duplicate-finder-client results

# Get results count
duplicate-finder-client results | wc -l
```

---

## 🛠️ Configuration

### Modify Scan Interval (in seconds)

```bash
# Edit daemon.c
nano daemon.c

# Find this line and change value:
# #define SCAN_INTERVAL 3600

# Recompile
make clean && make

# Reinstall
sudo make install-service

# Restart daemon
sudo systemctl restart duplicate-finder.service
```

### Common Intervals

```c
#define SCAN_INTERVAL 900    // 15 minutes
#define SCAN_INTERVAL 1800   // 30 minutes
#define SCAN_INTERVAL 3600   // 1 hour (default)
#define SCAN_INTERVAL 7200   // 2 hours
#define SCAN_INTERVAL 86400  // 1 day
```

---

## 🐛 Debugging & Troubleshooting

### Run Daemon in Foreground

```bash
# Shows all output directly (no daemonization)
./duplicate-finder-daemon --foreground

# Shows debug info (exit with Ctrl+C)
```

### Check Log Files

```bash
# Real-time logs
tail -f /var/log/syslog | grep duplicate-finder

# Or with systemd
journalctl -u duplicate-finder.service -f

# Check for errors
journalctl -u duplicate-finder.service | grep ERROR
```

### Verify Installation

```bash
# Check if executables exist
which duplicate-finder-client
which duplicate-finder-daemon
ls -la /usr/local/bin/duplicate*

# Check socket exists
ls -la /tmp/duplicate-finder.sock

# Check PID file
cat /var/run/duplicate-finder.pid

# Check daemon is running
ps aux | grep duplicate-finder
```

### Test Connectivity

```bash
# Try to connect to socket
nc -U /tmp/duplicate-finder.sock

# Check socket permissions
stat /tmp/duplicate-finder.sock
```

---

## 📁 File Locations

| Path | Purpose |
|------|---------|
| `/etc/duplicate-finder/` | Configuration (create if needed) |
| `/var/cache/duplicate-finder/` | Scan results and database |
| `/var/run/duplicate-finder.pid` | Daemon PID file |
| `/tmp/duplicate-finder.sock` | IPC socket |
| `/usr/local/bin/` | Installed executables |
| `/etc/systemd/system/duplicate-finder.service` | Service file |

---

## 🔧 Advanced Usage

### Performance Tuning

**Increase number of hash threads** (in duplicate_finder.c):
```c
// Change this line:
int num_threads = 4;
// To:
int num_threads = 8;  // or higher based on CPU cores
```

**Limit resource usage** (in service file):
```bash
# Edit service file
sudo nano /etc/systemd/system/duplicate-finder.service

# Add under [Service] section:
CPUQuota=50%
MemoryLimit=500M

# Reload
sudo systemctl daemon-reload
```

**Increase max files scanned** (in duplicate_finder.c):
```c
// Change this line:
#define MAX_FILES 100000
// To:
#define MAX_FILES 500000
```

---

## 📝 Common Tasks

### Export Results with Timestamp

```bash
duplicate-finder-client results > duplicates_$(date +%Y%m%d_%H%M%S).txt
```

### Compare Scans

```bash
# Run first scan
duplicate-finder-client results > scan1.txt

# Run another scan later
duplicate-finder-client results > scan2.txt

# Compare
diff scan1.txt scan2.txt
```

### Count Duplicate Sets

```bash
duplicate-finder-client results | grep "^Duplicate Set" | wc -l
```

### Calculate Total Wasted Space

```bash
duplicate-finder-client results | grep "Total wasted"
```

### Monitor in Real Time

```bash
# Update every 30 seconds
watch -n 30 'echo "=== Status ==="; \
              systemctl status duplicate-finder.service; \
              echo ""; \
              echo "=== Latest Results ==="; \
              duplicate-finder-client results | head -20'
```

---

## 🔓 Permission Issues

### If daemon won't start due to permissions:

```bash
# Check current permissions
ls -la /usr/local/bin/duplicate*

# Fix permissions
sudo chmod +x /usr/local/bin/duplicate-finder-daemon
sudo chmod +x /usr/local/bin/duplicate-finder-client

# Check service file permissions
sudo chmod 644 /etc/systemd/system/duplicate-finder.service

# Reload systemd
sudo systemctl daemon-reload
```

### If socket is inaccessible:

```bash
# Check socket permissions
stat /tmp/duplicate-finder.sock

# The socket should be world-readable/writable
# If not, restart daemon to recreate:
sudo systemctl restart duplicate-finder.service
```

---

## 🗑️ Uninstall & Cleanup

### Complete Removal

```bash
# Stop the service
sudo systemctl stop duplicate-finder.service

# Disable on boot
sudo systemctl disable duplicate-finder.service

# Remove executables
sudo rm -f /usr/local/bin/duplicate-finder*

# Remove service file
sudo rm -f /etc/systemd/system/duplicate-finder.service

# Reload systemd
sudo systemctl daemon-reload

# Remove cache/data
sudo rm -rf /var/cache/duplicate-finder

# Clean local build
make clean

# Verify removal
which duplicate-finder-daemon  # Should show "not found"
```

---

## 📊 Performance Optimization Checklist

- [ ] Use SSD instead of HDD
- [ ] Increase thread count for more cores
- [ ] Exclude slow network mounts
- [ ] Run on system with sufficient RAM
- [ ] Consider scanning during off-peak hours
- [ ] Use longer scan intervals for large systems
- [ ] Enable CPU quota limits to prevent system impact

---

## 💡 Tips & Tricks

### Automate Manual Scans with Cron

```bash
# Edit crontab
crontab -e

# Add this line to run scan daily at 2 AM:
0 2 * * * /usr/local/bin/duplicate-finder-client scan

# List crontab
crontab -l
```

### Get Notifications When Duplicates Found

```bash
# Create a wrapper script
#!/bin/bash
results=$(duplicate-finder-client results)
if [[ $results == *"Duplicate Set"* ]]; then
    echo "Duplicates found!" | mail -s "Duplicate Alert" your@email.com
fi

# Make executable and add to cron
chmod +x check_duplicates.sh
crontab -e  # Add: 0 3 * * * /path/to/check_duplicates.sh
```

---

## 📞 Getting Help

```bash
# Show all available commands
make help

# Show client help
duplicate-finder-client help

# View full documentation
cat ALGORITHM_AND_DESIGN.md
cat IMPLEMENTATION_GUIDE.md

# Check system logs
journalctl -u duplicate-finder.service -n 100
```

---

Created: May 22, 2026
Last Updated: May 22, 2026

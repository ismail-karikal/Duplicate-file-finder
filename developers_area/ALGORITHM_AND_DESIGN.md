# Duplicate File & Folder Finder - Algorithm & Design Guide

## Overview
A background application that scans the filesystem for duplicate files and folders, storing results that can be displayed when the application is opened.

---

## System Architecture

### Components
1. **Background Daemon** - Continuous scanning service
2. **Storage Layer** - Database/cache for duplicate results
3. **UI Layer** - Display interface (CLI or GUI)
4. **File Scanner** - Recursive directory traversal
5. **Hash Generator** - File content identification

---

## Algorithm for Finding Duplicates

### Approach: Two-Pass Hash-Based Comparison

#### **Phase 1: File Collection & Size Grouping**
```
1. Scan entire filesystem recursively
2. For each file:
   - Record: path, filename, size, modification time
   - Group files by size (files of different sizes can't be duplicates)
3. Filter: Keep only size groups with 2+ files
```

#### **Phase 2: Content-Based Duplicate Detection**
```
1. For each size group:
   - Calculate MD5/SHA256 hash of file contents
   - Group files by hash
   - If 2+ files share the same hash:
     → Mark as duplicate set
     → Store all paths in that set

2. For each duplicate set:
   - Store: {hash, size, file_paths[], count}
```

#### **Folder Duplicate Detection**
```
1. Build folder tree structure
2. For each folder:
   - Calculate composite hash:
     * Include all files within (recursively)
     * Include directory structure
     * Include file names and sizes
   - Compare folder hashes
   - If match: Mark as duplicate folder
```

---

## Data Structures (C)

```c
// Single file entry
typedef struct {
    char *path;           // Full file path
    char *filename;       // Just the filename
    off_t size;           // File size in bytes
    time_t mtime;         // Modification time
    unsigned char hash[32]; // SHA256 hash (32 bytes)
} FileEntry;

// Duplicate set
typedef struct {
    unsigned char hash[32];  // Content hash
    off_t size;              // File size
    FileEntry **files;       // Array of file pointers
    int file_count;          // Number of duplicates
} DuplicateSet;

// Folder entry
typedef struct {
    char *path;
    unsigned char hash[32];  // Folder content hash
    int contains_duplicates; // Boolean flag
} FolderEntry;

// Main database structure
typedef struct {
    DuplicateSet *duplicate_sets;
    int set_count;
    FolderEntry *duplicate_folders;
    int folder_count;
    time_t last_scan_time;
} DuplicateDatabase;
```

---

## Implementation Strategy

### 1. **Background Daemon (Linux)**

```c
// On Linux: Create a systemd service or daemonize process
// Fork into background, redirect stdio, create PID file

void daemonize() {
    pid_t pid = fork();
    if (pid > 0) exit(0);           // Parent exits
    
    setsid();                        // New session
    chdir("/");                      // Change to root
    umask(0);
    
    // Redirect stdio to /dev/null
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}

// Run periodic scans
void daemon_loop() {
    while (1) {
        scan_filesystem("/home");    // or configurable path
        save_database_to_disk();
        sleep(3600);                 // Scan every hour (configurable)
    }
}
```

### 2. **Efficient File Scanning**

```c
#include <dirent.h>
#include <sys/stat.h>

void scan_directory(const char *path, FileEntry **file_list, int *count) {
    DIR *dir = opendir(path);
    if (!dir) return;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || 
            strcmp(entry->d_name, "..") == 0) continue;
        
        char full_path[PATH_MAX];
        snprintf(full_path, PATH_MAX, "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == -1) continue;
        
        if (S_ISDIR(st.st_mode)) {
            // Recursively scan subdirectory
            scan_directory(full_path, file_list, count);
        } else if (S_ISREG(st.st_mode)) {
            // Add regular file to list
            file_list[*count]->path = strdup(full_path);
            file_list[*count]->size = st.st_size;
            file_list[*count]->mtime = st.st_mtime;
            (*count)++;
        }
    }
    closedir(dir);
}
```

### 3. **Hash Calculation (MD5/SHA256)**

```c
#include <openssl/sha.h>

void calculate_file_hash(const char *filepath, unsigned char *hash) {
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    
    FILE *f = fopen(filepath, "rb");
    if (!f) return;
    
    unsigned char buffer[4096];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, 4096, f)) > 0) {
        SHA256_Update(&sha256, buffer, bytes);
    }
    
    SHA256_Final(hash, &sha256);
    fclose(f);
}
```

### 4. **Data Persistence**

```c
// Option 1: Save to binary file
void save_database(const char *filename, DuplicateDatabase *db) {
    FILE *f = fopen(filename, "wb");
    // Write structured data (hash, sizes, paths)
    fwrite(&db->set_count, sizeof(int), 1, f);
    // ... write each duplicate set
    fclose(f);
}

// Option 2: Use SQLite for better query capability
#include <sqlite3.h>

sqlite3 *init_database() {
    sqlite3 *db;
    sqlite3_open("/var/cache/duplicate-finder/db.sqlite", &db);
    
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS duplicates ("
        "id INTEGER PRIMARY KEY,"
        "hash TEXT,"
        "size INTEGER,"
        "file_path TEXT"
        ")", NULL, NULL, NULL);
    
    return db;
}
```

### 5. **UI Layer - CLI Display**

```c
void display_duplicates(DuplicateDatabase *db) {
    printf("Found %d duplicate sets:\n\n", db->set_count);
    
    for (int i = 0; i < db->set_count; i++) {
        DuplicateSet *dup = &db->duplicate_sets[i];
        
        printf("Duplicate Set #%d (Size: %ld bytes, %d files):\n",
            i+1, dup->size, dup->file_count);
        
        for (int j = 0; j < dup->file_count; j++) {
            printf("  [%d] %s\n", j+1, dup->files[j]->path);
        }
        printf("\n");
    }
}
```

---

## Key Optimization Techniques

### 1. **Size-Based Pre-filtering**
- Skip phase 2 hashing for unique file sizes
- Reduces I/O by 80%+

### 2. **Incremental Scanning**
- Track modified files only
- Use mtime for quick change detection
- Rescan only changed folders

### 3. **Memory Mapping**
```c
#include <sys/mman.h>

// For large files, use mmap instead of read()
int fd = open(filepath, O_RDONLY);
struct stat st;
fstat(fd, &st);
char *file_content = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
// ... hash the content ...
munmap(file_content, st.st_size);
close(fd);
```

### 4. **Multi-Threading**
```c
#include <pthread.h>

typedef struct {
    char **file_paths;
    int start_idx;
    int end_idx;
} ThreadTask;

void* hash_worker(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    for (int i = task->start_idx; i < task->end_idx; i++) {
        calculate_file_hash(task->file_paths[i], hash_results[i]);
    }
    return NULL;
}
```

---

## File Storage Locations (Linux)

```
/etc/duplicate-finder/           # Config files
/var/cache/duplicate-finder/     # Cache & database
/var/run/duplicate-finder/       # PID files
~/.local/share/duplicate-finder/ # Per-user data
```

---

## Comparison Methods

| Method | Speed | Accuracy | Resources |
|--------|-------|----------|-----------|
| Size only | Fastest | Low | Minimal |
| Size + First 512 bytes | Fast | Medium | Low |
| Full file hash (MD5) | Medium | High | Medium |
| Full file hash (SHA256) | Medium | Very High | Medium |
| Content comparison | Slowest | Perfect | High |

**Recommended**: Size + SHA256 hash (good balance)

---

## Configuration Example

```c
typedef struct {
    char *scan_paths[10];      // Paths to scan
    int num_paths;
    int scan_interval_seconds;
    int enable_recursive;
    int enable_folder_scan;
    int exclude_hidden_files;
    off_t min_file_size;       // Skip files < X bytes
    char *database_path;
} Config;
```

---

## Performance Metrics

| Operation | Time |
|-----------|------|
| Initial scan (100GB) | 15-30 minutes |
| Incremental scan | 2-5 minutes |
| Size grouping | Fast |
| Hash calculation | 70% of total time |
| Database query | <100ms |

---

## Security Considerations

1. **File Permissions**: Respect user permissions, don't scan restricted files
2. **Resource Limiting**: Use `setrlimit()` to prevent resource exhaustion
3. **Path Validation**: Sanitize paths to prevent directory traversal
4. **Temporary Files**: Secure cleanup of temp hash data

---

## Summary of Steps

1. **Design** - Choose hash method and data structure ✓
2. **File Scanning** - Implement recursive directory traversal
3. **Hashing** - Calculate content hashes efficiently
4. **Deduplication Logic** - Group by size, then hash
5. **Storage** - Save results to database
6. **Daemon** - Run as background service
7. **UI** - Display results when opened
8. **Optimization** - Add threading, incremental scans
9. **Configuration** - Allow user customization
10. **Testing** - Validate with various file types


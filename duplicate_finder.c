#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <limits.h>

#define MAX_FILES 100000
#define MAX_PATH 4096
#define BUFFER_SIZE 4096

// Data Structures
typedef struct {
    char path[MAX_PATH];
    off_t size;
    time_t mtime;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    int hash_calculated;
} FileEntry;

typedef struct {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    off_t size;
    FileEntry **files;
    int file_count;
} DuplicateSet;

typedef struct {
    FileEntry *files;
    int file_count;
    DuplicateSet *duplicates;
    int duplicate_count;
} FileDatabase;

// Global variables
FileDatabase db = {0};
pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;

// ============ FUNCTION PROTOTYPES ============

void scan_directory(const char *path);
void calculate_file_hash(const char *filepath, unsigned char *hash);
void find_duplicates();
void display_duplicates();
void cleanup();

// ============ FILE SCANNING ============

/**
 * Recursively scan directory and collect all files
 */
void scan_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Cannot open directory");
        return;
    }

    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || 
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Skip hidden files (optional)
        if (entry->d_name[0] == '.') {
            continue;
        }

        // Build full path
        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) == -1) {
            continue;
        }

        // If directory, recurse
        if (S_ISDIR(st.st_mode)) {
            scan_directory(full_path);
        } 
        // If regular file, add to database
        else if (S_ISREG(st.st_mode)) {
            if (db.file_count >= MAX_FILES) {
                fprintf(stderr, "Maximum file limit reached\n");
                continue;
            }

            FileEntry *entry = &db.files[db.file_count];
            strncpy(entry->path, full_path, MAX_PATH - 1);
            entry->size = st.st_size;
            entry->mtime = st.st_mtime;
            entry->hash_calculated = 0;

            db.file_count++;

            // Progress indicator
            if (db.file_count % 1000 == 0) {
                printf("Scanned %d files...\n", db.file_count);
            }
        }
    }

    closedir(dir);
}

// ============ HASHING ============

/**
 * Calculate SHA256 hash of file contents
 */
void calculate_file_hash(const char *filepath, unsigned char *hash) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        return;
    }

    SHA256_CTX sha256_ctx;
    SHA256_Init(&sha256_ctx);

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes;

    while ((bytes = fread(buffer, 1, BUFFER_SIZE, f)) > 0) {
        SHA256_Update(&sha256_ctx, buffer, bytes);
    }

    SHA256_Final(hash, &sha256_ctx);
    fclose(f);
}

/**
 * Thread worker for parallel hashing
 */
typedef struct {
    int start_idx;
    int end_idx;
} HashTask;

void* hash_worker(void *arg) {
    HashTask *task = (HashTask *)arg;

    for (int i = task->start_idx; i < task->end_idx; i++) {
        FileEntry *entry = &db.files[i];
        
        if (!entry->hash_calculated) {
            calculate_file_hash(entry->path, entry->hash);
            entry->hash_calculated = 1;

            if ((i - task->start_idx) % 100 == 0) {
                printf("  Thread: Hashed %d files in range [%d-%d]\n", 
                    i - task->start_idx, task->start_idx, task->end_idx);
            }
        }
    }

    free(task);
    return NULL;
}

/**
 * Calculate hashes for all files using multiple threads
 */
void calculate_all_hashes() {
    printf("Calculating hashes for %d files...\n", db.file_count);

    int num_threads = 4;  // Can be adjusted based on CPU cores
    pthread_t threads[num_threads];

    int files_per_thread = db.file_count / num_threads;

    for (int i = 0; i < num_threads; i++) {
        HashTask *task = malloc(sizeof(HashTask));
        task->start_idx = i * files_per_thread;
        task->end_idx = (i == num_threads - 1) ? db.file_count : 
                        (i + 1) * files_per_thread;

        pthread_create(&threads[i], NULL, hash_worker, (void *)task);
    }

    // Wait for all threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Hash calculation complete!\n");
}

// ============ DUPLICATE DETECTION ============

/**
 * Compare two hashes
 */
int hash_compare(const unsigned char *h1, const unsigned char *h2) {
    return memcmp(h1, h2, SHA256_DIGEST_LENGTH);
}

/**
 * Sort files by size for grouping
 */
int compare_by_size(const void *a, const void *b) {
    FileEntry *fa = (FileEntry *)a;
    FileEntry *fb = (FileEntry *)b;
    
    if (fa->size < fb->size) return -1;
    if (fa->size > fb->size) return 1;
    return 0;
}

/**
 * Find duplicate files
 * Algorithm:
 * 1. Sort by size
 * 2. Group files with same size
 * 3. For each group with 2+ files, compare hashes
 * 4. Store duplicate sets
 */
void find_duplicates() {
    printf("Finding duplicates...\n");

    // Phase 1: Sort by size
    qsort(db.files, db.file_count, sizeof(FileEntry), compare_by_size);

    db.duplicates = malloc(sizeof(DuplicateSet) * db.file_count);
    db.duplicate_count = 0;

    // Phase 2: Group by size and find duplicates
    int i = 0;
    while (i < db.file_count) {
        off_t current_size = db.files[i].size;
        int group_start = i;

        // Find all files with same size
        while (i < db.file_count && db.files[i].size == current_size) {
            i++;
        }

        int group_size = i - group_start;

        // Only process groups with 2+ files
        if (group_size >= 2) {
            // Sub-sort by hash
            qsort(&db.files[group_start], group_size, 
                  sizeof(FileEntry), compare_by_size);

            // Group by hash within this size group
            int j = group_start;
            while (j < i) {
                unsigned char *current_hash = db.files[j].hash;
                int hash_group_start = j;

                while (j < i && 
                       hash_compare(db.files[j].hash, current_hash) == 0) {
                    j++;
                }

                int hash_group_size = j - hash_group_start;

                // If 2+ files with same hash, it's a duplicate set
                if (hash_group_size >= 2) {
                    DuplicateSet *dup_set = 
                        &db.duplicates[db.duplicate_count];
                    
                    memcpy(dup_set->hash, current_hash, SHA256_DIGEST_LENGTH);
                    dup_set->size = current_size;
                    dup_set->file_count = hash_group_size;
                    dup_set->files = malloc(sizeof(FileEntry *) * hash_group_size);

                    for (int k = 0; k < hash_group_size; k++) {
                        dup_set->files[k] = &db.files[hash_group_start + k];
                    }

                    db.duplicate_count++;
                }
            }
        }
    }

    printf("Found %d duplicate sets\n", db.duplicate_count);
}

// ============ DISPLAY ============

/**
 * Display found duplicates
 */
void display_duplicates() {
    if (db.duplicate_count == 0) {
        printf("\nNo duplicates found!\n");
        return;
    }

    printf("\n========== DUPLICATE FILES FOUND ==========\n\n");

    off_t total_wasted = 0;

    for (int i = 0; i < db.duplicate_count; i++) {
        DuplicateSet *dup = &db.duplicates[i];

        printf("Duplicate Set #%d:\n", i + 1);
        printf("  Size: %ld bytes\n", dup->size);
        printf("  Count: %d files\n", dup->file_count);
        printf("  Hash: ");
        for (int j = 0; j < 16; j++) {  // Print first 16 bytes
            printf("%02x", dup->hash[j]);
        }
        printf("...\n");
        printf("  Files:\n");

        for (int j = 0; j < dup->file_count; j++) {
            printf("    [%d] %s\n", j + 1, dup->files[j]->path);
        }

        // Calculate wasted space (all but one copy)
        off_t wasted = dup->size * (dup->file_count - 1);
        total_wasted += wasted;

        printf("  Wasted space: %ld bytes\n\n", wasted);
    }

    printf("Total wasted space: %ld bytes (%.2f MB)\n", 
        total_wasted, total_wasted / (1024.0 * 1024.0));
    printf("==========================================\n");
}

// ============ CLEANUP ============

void cleanup() {
    for (int i = 0; i < db.duplicate_count; i++) {
        free(db.duplicates[i].files);
    }
    free(db.duplicates);
    free(db.files);
}

// ============ MAIN ============

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <path_to_scan> [path2] [path3] ...\n", argv[0]);
        printf("Example: %s /home /var\n", argv[0]);
        return 1;
    }

    // Allocate file database
    db.files = malloc(sizeof(FileEntry) * MAX_FILES);
    if (!db.files) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    printf("Starting duplicate file scanner...\n");
    printf("========================================\n\n");

    // Scan all provided paths
    for (int i = 1; i < argc; i++) {
        printf("Scanning: %s\n", argv[i]);
        scan_directory(argv[i]);
    }

    printf("\nTotal files found: %d\n\n", db.file_count);

    // Calculate hashes
    calculate_all_hashes();

    // Find duplicates
    printf("\n");
    find_duplicates();

    // Display results
    printf("\n");
    display_duplicates();

    // Cleanup
    cleanup();

    printf("\nScanning complete!\n");
    return 0;
}

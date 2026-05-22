#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>

#define PID_FILE "/var/run/duplicate-finder.pid"
#define SOCKET_FILE "/tmp/duplicate-finder.sock"
#define DB_FILE "/var/cache/duplicate-finder/results.db"
#define SCAN_INTERVAL 3600  // 1 hour in seconds

volatile int running = 1;

// ============ SIGNAL HANDLING ============

void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        syslog(LOG_INFO, "Received shutdown signal");
        running = 0;
    }
}

// ============ DAEMONIZATION ============

/**
 * Daemonize the process
 * - Fork into background
 * - Disconnect from terminal
 * - Create new session
 * - Redirect I/O to /dev/null
 */
void daemonize() {
    pid_t pid = fork();
    
    // Parent process exits
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // Fork failed
    if (pid < 0) {
        syslog(LOG_ERR, "First fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Create new session (detach from terminal)
    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid() failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Second fork to ensure daemon cannot acquire controlling terminal
    pid = fork();
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    if (pid < 0) {
        syslog(LOG_ERR, "Second fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Change working directory to root
    if (chdir("/") < 0) {
        syslog(LOG_ERR, "chdir() failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Clear file creation mask
    umask(0);

    // Redirect standard file descriptors to /dev/null
    int devnull = open("/dev/null", O_RDWR);
    if (devnull < 0) {
        syslog(LOG_ERR, "Cannot open /dev/null: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);

    close(devnull);
}

// ============ PID FILE MANAGEMENT ============

void write_pid_file() {
    FILE *f = fopen(PID_FILE, "w");
    if (f == NULL) {
        syslog(LOG_ERR, "Cannot write PID file: %s", strerror(errno));
        return;
    }
    fprintf(f, "%d\n", getpid());
    fclose(f);
}

void remove_pid_file() {
    unlink(PID_FILE);
}

// ============ DIRECTORY SETUP ============

void setup_directories() {
    char cmd[256];

    // Create cache directory
    snprintf(cmd, sizeof(cmd), "mkdir -p /var/cache/duplicate-finder");
    system(cmd);

    // Set permissions
    snprintf(cmd, sizeof(cmd), "chmod 755 /var/cache/duplicate-finder");
    system(cmd);

    syslog(LOG_INFO, "Directories created");
}

// ============ SCANNING LOGIC ============

void perform_scan() {
    time_t now = time(NULL);
    char time_str[26];
    ctime_r(&now, time_str);
    time_str[strcspn(time_str, "\n")] = 0;

    syslog(LOG_INFO, "Starting filesystem scan at %s", time_str);

    // Here you would call the file scanning logic
    // For now, this is a placeholder
    
    // Example: scan /home directory
    system("./duplicate_finder /home > /var/cache/duplicate-finder/results.txt 2>&1");

    syslog(LOG_INFO, "Filesystem scan completed");
}

// ============ COMMUNICATION SOCKET ============

/**
 * Create Unix domain socket for client communication
 * Allows the GUI/CLI to request results from daemon
 */
int create_socket() {
    unlink(SOCKET_FILE);  // Remove old socket file

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        syslog(LOG_ERR, "socket() failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_FILE, sizeof(addr.sun_path) - 1);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "bind() failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    if (listen(sock, 5) < 0) {
        syslog(LOG_ERR, "listen() failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    // Set permissions so users can connect
    chmod(SOCKET_FILE, 0777);

    syslog(LOG_INFO, "Communication socket created at %s", SOCKET_FILE);
    return sock;
}

/**
 * Handle client connections
 * Clients can request:
 * - "GET_RESULTS" - Get duplicate findings
 * - "START_SCAN" - Manually trigger a scan
 * - "STATUS" - Get daemon status
 */
void* socket_server(void *arg) {
    int server_sock = (intptr_t)arg;

    while (running) {
        struct sockaddr_un addr;
        socklen_t addr_len = sizeof(addr);

        // Accept connection (with timeout)
        int client_sock = accept(server_sock, (struct sockaddr *)&addr, &addr_len);
        if (client_sock < 0) {
            if (errno != EINTR) {
                syslog(LOG_ERR, "accept() failed: %s", strerror(errno));
            }
            continue;
        }

        char buffer[256];
        ssize_t n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

        if (n > 0) {
            buffer[n] = '\0';

            if (strcmp(buffer, "GET_RESULTS") == 0) {
                // Send results file to client
                FILE *f = fopen(DB_FILE, "r");
                if (f) {
                    char line[512];
                    while (fgets(line, sizeof(line), f)) {
                        send(client_sock, line, strlen(line), 0);
                    }
                    fclose(f);
                }
            } 
            else if (strcmp(buffer, "START_SCAN") == 0) {
                syslog(LOG_INFO, "Manual scan requested by client");
                perform_scan();
                send(client_sock, "SCAN_STARTED", 12, 0);
            }
            else if (strcmp(buffer, "STATUS") == 0) {
                time_t now = time(NULL);
                char response[128];
                snprintf(response, sizeof(response), "DAEMON_RUNNING|PID:%d|TIME:%ld",
                    getpid(), now);
                send(client_sock, response, strlen(response), 0);
            }
        }

        close(client_sock);
    }

    close(server_sock);
    return NULL;
}

// ============ MAIN DAEMON LOOP ============

void daemon_main() {
    time_t last_scan = 0;

    syslog(LOG_INFO, "Daemon starting (PID: %d)", getpid());

    // Create communication socket
    int server_sock = create_socket();
    if (server_sock < 0) {
        syslog(LOG_ERR, "Failed to create socket");
        return;
    }

    // Start socket server thread
    pthread_t socket_thread;
    pthread_create(&socket_thread, NULL, socket_server, (void *)(intptr_t)server_sock);

    // Main loop
    while (running) {
        time_t now = time(NULL);

        // Perform scan at intervals
        if (now - last_scan >= SCAN_INTERVAL) {
            perform_scan();
            last_scan = now;
        }

        sleep(60);  // Check every minute if scan is needed
    }

    syslog(LOG_INFO, "Daemon shutting down");
}

// ============ MAIN ============

int main(int argc, char *argv[]) {
    // Open syslog
    openlog("duplicate-finder", LOG_PID | LOG_CONS, LOG_DAEMON);

    // Handle command line arguments
    if (argc > 1) {
        if (strcmp(argv[1], "--stop") == 0) {
            // Kill existing daemon
            FILE *f = fopen(PID_FILE, "r");
            if (f) {
                pid_t pid;
                fscanf(f, "%d", &pid);
                fclose(f);
                kill(pid, SIGTERM);
                printf("Daemon stopped\n");
                return 0;
            }
            printf("Daemon not running\n");
            return 1;
        }
        else if (strcmp(argv[1], "--status") == 0) {
            // Check if daemon is running
            FILE *f = fopen(PID_FILE, "r");
            if (f) {
                pid_t pid;
                fscanf(f, "%d", &pid);
                fclose(f);
                if (kill(pid, 0) == 0) {
                    printf("Daemon is running (PID: %d)\n", pid);
                    return 0;
                }
            }
            printf("Daemon is not running\n");
            return 1;
        }
        else if (strcmp(argv[1], "--foreground") == 0) {
            // Run in foreground (for debugging)
            syslog(LOG_INFO, "Starting in foreground mode");
            setup_directories();
            signal(SIGTERM, signal_handler);
            signal(SIGINT, signal_handler);
            daemon_main();
            closelog();
            return 0;
        }
    }

    // Check if already running
    FILE *f = fopen(PID_FILE, "r");
    if (f) {
        pid_t existing_pid;
        fscanf(f, "%d", &existing_pid);
        fclose(f);
        if (kill(existing_pid, 0) == 0) {
            syslog(LOG_ERR, "Daemon already running (PID: %d)", existing_pid);
            fprintf(stderr, "Error: Daemon already running (PID: %d)\n", existing_pid);
            return 1;
        }
    }

    // Setup directories
    setup_directories();

    // Setup signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, SIG_IGN);

    // Daemonize
    daemonize();

    // Write PID file
    write_pid_file();

    // Run daemon
    daemon_main();

    // Cleanup
    remove_pid_file();
    closelog();

    return 0;
}

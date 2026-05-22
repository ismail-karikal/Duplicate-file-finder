#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>

#define SOCKET_FILE "/tmp/duplicate-finder.sock"
#define BUFFER_SIZE 4096

/**
 * Connect to daemon socket and send command
 */
int send_command(const char *command) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_FILE, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error: Cannot connect to daemon.\n");
        fprintf(stderr, "Make sure the daemon is running: duplicate-finder-daemon\n");
        close(sock);
        return -1;
    }

    // Send command
    if (send(sock, command, strlen(command), 0) < 0) {
        perror("send");
        close(sock);
        return -1;
    }

    // Receive response
    char buffer[BUFFER_SIZE];
    ssize_t n;

    printf("Response from daemon:\n");
    printf("======================\n");

    while ((n = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, n, stdout);
    }

    printf("\n======================\n");

    close(sock);
    return 0;
}

void print_usage(const char *prog) {
    printf("Duplicate File Finder - Client\n");
    printf("===============================\n\n");
    printf("Usage: %s <command>\n\n", prog);
    printf("Commands:\n");
    printf("  status        - Check if daemon is running\n");
    printf("  results       - Get latest scan results\n");
    printf("  scan          - Request immediate scan\n");
    printf("  help          - Show this help message\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "status") == 0) {
        printf("Checking daemon status...\n");
        send_command("STATUS");
    }
    else if (strcmp(cmd, "results") == 0) {
        printf("Retrieving scan results...\n");
        send_command("GET_RESULTS");
    }
    else if (strcmp(cmd, "scan") == 0) {
        printf("Requesting immediate scan...\n");
        send_command("START_SCAN");
    }
    else if (strcmp(cmd, "help") == 0) {
        print_usage(argv[0]);
    }
    else {
        printf("Unknown command: %s\n\n", cmd);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}

#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT_NUM 3456
#define MAX_LINE 1024
#define MAX_FILES 200
#define BUFFER_SIZE 4096

typedef struct {
    char name[256];
    long size;
} FileInfo;

/*
 * Connect to the given hostname on PORT_NUM using TCP.
 * Uses older gethostbyname style so it stays simple.
 */
int connect_to_server(const char *hostname) {
    struct hostent *he;
    struct sockaddr_in server;
    int sockfd;

    he = gethostbyname(hostname);
    if (!he) {
        fprintf(stderr, "Could not resolve host %s\n", hostname);
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT_NUM);
    memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sockfd, (struct sockaddr *)&server, sizeof server) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/*
 * Send a string followed by fflush so the server actually sees it.
 */
int send_line(FILE *sock, const char *line) {
    if (fputs(line, sock) == EOF) {
        return 0;
    }
    if (fflush(sock) == EOF) {
        return 0;
    }
    return 1;
}

/*
 * Read a line from the socket and strip the newline at the end.
 */
int read_line(FILE *sock, char *buf, size_t size) {
    if (fgets(buf, size, sock) == NULL) {
        return 0;
    }
    // strip newline if present
    size_t len = strlen(buf);
    if (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
        buf[len-1] = '\0';
        len--;
        if (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n')) {
            buf[len-1] = '\0';
        }
    }
    return 1;
}

void print_menu(void) {
    printf("\n==== File Download Client ====\n");
    printf("1) List files on server\n");
    printf("2) Download a file\n");
    printf("3) Download all files\n");
    printf("4) Quit\n");
    printf("Choice: ");
}

/*
 * LIST
 * Uses the "." end-of-data marker from the lecture.
 * Also stores the listing into an array for the "download all" feature.
 */
int get_file_list(FILE *sock, FileInfo files[], int max_files) {
    char line[MAX_LINE];
    int count = 0;

    if (!send_line(sock, "LIST\n")) {
        fprintf(stderr, "Error sending LIST command.\n");
        return -1;
    }

    // status line +OK or -ERR
    if (!read_line(sock, line, sizeof line)) {
        fprintf(stderr, "Error reading LIST response.\n");
        return -1;
    }
    if (strncmp(line, "+OK", 3) != 0) {
        fprintf(stderr, "LIST failed: %s\n", line);
        return -1;
    }

    printf("\nFiles on server:\n");
    printf("%-8s  %-40s\n", "Size", "Name");
    printf("---------------------------------------------------------------\n");

    while (1) {
        if (!read_line(sock, line, sizeof line)) {
            fprintf(stderr, "Connection lost while reading list.\n");
            return -1;
        }
        if (strcmp(line, ".") == 0) {
            break; // end-of-data marker
        }

        long size;
        char name[256];
        if (sscanf(line, "%ld %255s", &size, name) == 2) {
            printf("%-8ld  %-40s\n", size, name);
            if (count < max_files) {
                files[count].size = size;
                strncpy(files[count].name, name, sizeof(files[count].name));
                files[count].name[sizeof(files[count].name)-1] = '\0';
                count++;
            }
        } else {
            // if something weird comes back, just show it raw
            printf("???  %s\n", line);
        }
    }

    if (count == 0) {
        printf("(no files)\n");
    }

    return count;
}

/*
 * Helper to see if a local file already exists so we can ask
 * the user about overwriting it.
 */
int file_exists(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

/*
 * Download a single file using SIZE + GET.
 * Uses length-prefix framing + chunked fread/fwrite with a small buffer.
 */
int download_file(FILE *sock, const char *filename) {
    char line[MAX_LINE];
    char cmd[MAX_LINE];
    long size;

    // Ask for size first (length-prefix framing)
    snprintf(cmd, sizeof cmd, "SIZE %s\n", filename);
    if (!send_line(sock, cmd)) {
        fprintf(stderr, "Error sending SIZE command.\n");
        return 0;
    }

    if (!read_line(sock, line, sizeof line)) {
        fprintf(stderr, "Error reading SIZE response.\n");
        return 0;
    }

    if (strncmp(line, "+OK", 3) != 0) {
        fprintf(stderr, "SIZE failed: %s\n", line);
        return 0;
    }

    if (sscanf(line, "+OK %ld", &size) != 1) {
        fprintf(stderr, "Could not parse size from: %s\n", line);
        return 0;
    }

    printf("File size for %s: %ld bytes\n", filename, size);

    if (size <= 0) {
        fprintf(stderr, "Invalid file size.\n");
        return 0;
    }

    // Overwrite check
    if (file_exists(filename)) {
        char answer[8];
        printf("File '%s' already exists. Overwrite? (y/n): ", filename);
        if (!fgets(answer, sizeof answer, stdin)) {
            return 0;
        }
        if (answer[0] != 'y' && answer[0] != 'Y') {
            printf("Skipping download of %s.\n", filename);
            // skip GET, just return success
            return 1;
        }
    }

    // Send GET command
    snprintf(cmd, sizeof cmd, "GET %s\n", filename);
    if (!send_line(sock, cmd)) {
        fprintf(stderr, "Error sending GET command.\n");
        return 0;
    }

    // Read +OK line before raw data
    if (!read_line(sock, line, sizeof line)) {
        fprintf(stderr, "Error reading GET response.\n");
        return 0;
    }
    if (strncmp(line, "+OK", 3) != 0) {
        fprintf(stderr, "GET failed: %s\n", line);
        return 0;
    }

    FILE *out = fopen(filename, "wb");
    if (!out) {
        perror("fopen");
        return 0;
    }

    long remaining = size;
    long received_total = 0;
    unsigned char buffer[BUFFER_SIZE];

    while (remaining > 0) {
        size_t to_read = remaining < BUFFER_SIZE ? (size_t)remaining : BUFFER_SIZE;
        size_t got = fread(buffer, 1, to_read, sock);
        if (got == 0) {
            fprintf(stderr, "\nConnection lost while downloading.\n");
            fclose(out);
            return 0;
        }

        size_t written = fwrite(buffer, 1, got, out);
        if (written != got) {
            fprintf(stderr, "\nError writing to file.\n");
            fclose(out);
            return 0;
        }

        remaining -= got;
        received_total += got;

        // simple progress bar
        int percent = (int)((received_total * 100) / size);
        printf("\rDownloading %s: %3d%%", filename, percent);
        fflush(stdout);
    }

    printf("\nDownload complete: %s (%ld bytes)\n", filename, size);
    fclose(out);
    return 1;
}

int main(void) {
    char host_choice[16];
    char hostname[64];
    int sockfd;
    FILE *sock;

    printf("Connect to which server?\n");
    printf("1) richmond.cs.sierracollege.edu\n");
    printf("2) london.cs.sierracollege.edu\n");
    printf("Enter 1 or 2: ");
    if (!fgets(host_choice, sizeof host_choice, stdin)) {
        return 1;
    }

    if (host_choice[0] == '2') {
        strcpy(hostname, "london.cs.sierracollege.edu");
    } else {
        strcpy(hostname, "richmond.cs.sierracollege.edu");
    }

    sockfd = connect_to_server(hostname);
    if (sockfd < 0) {
        return 1;
    }

    sock = fdopen(sockfd, "r+");
    if (!sock) {
        perror("fdopen");
        close(sockfd);
        return 1;
    }
    setvbuf(sock, NULL, _IONBF, 0);

    char line[MAX_LINE];

    if (!read_line(sock, line, sizeof line)) {
        fprintf(stderr, "Did not receive greeting from server.\n");
        fclose(sock);
        return 1;
    }
    printf("Server says: %s\n", line);

    int running = 1;
    FileInfo files[MAX_FILES];
    int file_count = 0;

    while (running) {
        print_menu();
        char choice_line[16];
        if (!fgets(choice_line, sizeof choice_line, stdin)) {
            break;
        }
        int choice = atoi(choice_line);

        if (choice == 1) {
            file_count = get_file_list(sock, files, MAX_FILES);
        } else if (choice == 2) {
            char filename[256];
            printf("Enter filename to download: ");
            if (!fgets(filename, sizeof filename, stdin)) {
                continue;
            }

            size_t len = strlen(filename);
            if (len > 0 && (filename[len-1] == '\n' || filename[len-1] == '\r')) {
                filename[len-1] = '\0';
            }
            if (filename[0] == '\0') {
                printf("No filename entered.\n");
                continue;
            }
            download_file(sock, filename);

        } else if (choice == 3) {
            if (file_count <= 0) {
                printf("You need to list files first.\n");
                continue;
            }
            for (int i = 0; i < file_count; i++) {
                printf("\n--- Downloading %s ---\n", files[i].name);
                download_file(sock, files[i].name);
            }

        } else if (choice == 4) {
            if (!send_line(sock, "QUIT\n")) {
                fprintf(stderr, "Error sending QUIT.\n");
            } else if (read_line(sock, line, sizeof line)) {
                printf("Server says: %s\n", line);
            }
            running = 0;

        } else {
            printf("Invalid choice.\n");
        }
    }

    fclose(sock);
    return 0;
}

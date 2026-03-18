#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 1024

void get_timestamp(char *timestamp, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, size, "%Y-%m-%d %H:%M:%S", t);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <log_file>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    char *log_file = argv[2];

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(1);
    }

    printf("Student server listening on port %d...\n", port);
    printf("Logging to: %s\n\n", log_file);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        char buffer[BUFFER_SIZE];
        ssize_t bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';

            char timestamp[64];
            get_timestamp(timestamp, sizeof(timestamp));

            printf("Received from %s:\n", client_ip);
            printf("  Data: %s\n", buffer);
            printf("  Time: %s\n\n", timestamp);

            FILE *log = fopen(log_file, "a");
            if (log) {
                fprintf(log, "%s %s %s\n", client_ip, timestamp, buffer);
                fclose(log);
            } else {
                perror("Cannot open log file");
            }
        }

        close(client_sock);
    }

    close(server_sock);
    return 0;
}
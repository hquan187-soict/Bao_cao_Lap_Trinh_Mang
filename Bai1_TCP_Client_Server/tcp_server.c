#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

char* read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Cannot open greeting file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(size + 1);
    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);

    return content;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <greeting_file> <output_file>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    char *greeting_file = argv[2];
    char *output_file = argv[3];

    char *greeting = read_file(greeting_file);
    if (!greeting) {
        exit(1);
    }

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        free(greeting);
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
        free(greeting);
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        free(greeting);
        close(server_sock);
        exit(1);
    }

    printf("Server listening on port %d...\n", port);

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
        printf("Client connected: %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        send(client_sock, greeting, strlen(greeting), 0);

        FILE *output = fopen(output_file, "a");
        if (!output) {
            perror("Cannot open output file");
            close(client_sock);
            continue;
        }

        char buffer[BUFFER_SIZE];
        ssize_t bytes_received;
        
        while ((bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            buffer[bytes_received] = '\0';
            fprintf(output, "%s\n", buffer);
            fflush(output);
        }

        fclose(output);
        close(client_sock);
        printf("Client disconnected: %s\n", client_ip);
    }

    free(greeting);
    close(server_sock);
    return 0;
}
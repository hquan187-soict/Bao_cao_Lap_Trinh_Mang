#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <signal.h>
#include <sys/wait.h>

#define PORT 8080
#define BUFFER_SIZE 1024

char SHARED_FOLDER[] = "./";

void signal_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void send_file_list(int client) {
    DIR *dir = opendir(SHARED_FOLDER);
    
    if (dir == NULL) {
        char msg[] = "ERROR No files to download\r\n";
        send(client, msg, strlen(msg), 0);
        return;
    }

    struct dirent *entry;
    char response[4096] = "";
    char filenames[100][256];
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            strcpy(filenames[count], entry->d_name);
            count++;
        }
    }

    closedir(dir);

    if (count == 0) {
        char msg[] = "ERROR No files to download\r\n";
        send(client, msg, strlen(msg), 0);
        return;
    }

    sprintf(response, "OK %d\r\n", count);

    for (int i = 0; i < count; i++) {
        strcat(response, filenames[i]);
        strcat(response, "\r\n");
    }

    strcat(response, "\r\n");
    send(client, response, strlen(response), 0);
}

void handle_client(int client) {
    send_file_list(client);
    char filename[256];

    while (1) {
        memset(filename, 0, sizeof(filename));
        int len = recv(client, filename, sizeof(filename), 0);

        if (len <= 0) break;

        filename[strcspn(filename, "\r\n")] = 0;
        char filepath[512];
        sprintf(filepath, "%s/%s", SHARED_FOLDER, filename);
        FILE *f = fopen(filepath, "rb");

        if (f == NULL) {
            char err[] = "ERROR File not found\r\n";
            send(client, err, strlen(err), 0);
            continue;
        }

        fseek(f, 0, SEEK_END);
        int filesize = ftell(f);
        rewind(f);
        char header[128];
        sprintf(header, "OK %d\r\n", filesize);
        send(client, header, strlen(header), 0);
        char buffer[BUFFER_SIZE];

        while (!feof(f)) {
            int bytesRead = fread(buffer, 1, BUFFER_SIZE, f);

            if (bytesRead > 0) {
                send(client, buffer, bytesRead, 0);
            }
        }

        fclose(f);
        printf("Sent file: %s\n", filename);
        break;
    }

    close(client);
}

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(listener, 5) < 0) {
        perror("listen");
        return 1;
    }

    signal(SIGCHLD, signal_handler);

    printf("File server listening on port %d...\n", PORT);

    while (1) {
        int client = accept(listener, NULL, NULL);

        if (client < 0) continue;

        printf("New client connected\n");
        pid_t pid = fork();

        if (pid == 0) {
            close(listener);
            handle_client(client);
            exit(0);
        }

        close(client);
    }

    close(listener);

    return 0;
}
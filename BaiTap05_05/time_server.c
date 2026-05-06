#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

void sigchld_handler(int s) {
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void process_time_request(int client_socket) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }

    buffer[strcspn(buffer, "\r\n")] = 0;

    char response[1024];
    memset(response, 0, sizeof(response));

    if (strncmp(buffer, "GET_TIME ", 9) == 0) {
        char *format = buffer + 9; 

        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);

        if (strcmp(format, "dd/mm/yyyy") == 0) {
            strftime(response, sizeof(response), "%d/%m/%Y\n", timeinfo);
        } else if (strcmp(format, "dd/mm/yy") == 0) {
            strftime(response, sizeof(response), "%d/%m/%y\n", timeinfo);
        } else if (strcmp(format, "mm/dd/yyyy") == 0) {
            strftime(response, sizeof(response), "%m/%d/%Y\n", timeinfo);
        } else if (strcmp(format, "mm/dd/yy") == 0) {
            strftime(response, sizeof(response), "%m/%d/%y\n", timeinfo);
        } else {
            strcpy(response, "LOI: Dinh dang thoi gian khong ho tro. Vui long dung: dd/mm/yyyy, dd/mm/yy, mm/dd/yyyy, hoac mm/dd/yy.\n");
        }
    } else {
        strcpy(response, "LOI: Cu phap khong hop le. Vui long su dung: GET_TIME [format]\n");
    }
    send(client_socket, response, strlen(response), 0);
    close(client_socket);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Loi: Khong the tao socket");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(9090);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Loi: Bind that bai");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Loi: Listen that bai");
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, sigchld_handler);

    printf("=> Time Server dang hoat dong tren port 9090...\n");

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Loi: Accept that bai");
            continue;
        }

        printf("[INFO] Chap nhan ket noi moi tu client.\n");

        pid_t pid = fork();

        if (pid == 0) {
            close(server_socket);
            process_time_request(client_socket);
            exit(0);
        } else if (pid > 0) {
            close(client_socket);
        } else {
            perror("Loi: Fork that bai");
        }
    }

    close(server_socket);
    return 0;
}

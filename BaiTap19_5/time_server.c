#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

void *client_thread(void *arg);

int main() {

    int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server == -1) {
        perror("socket()");
        return 1;
    }

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind()");
        close(server);
        return 1;
    }

    if (listen(server, 10)) {
        perror("listen()");
        close(server);
        return 1;
    }

    printf("Time server dang chay cong 8080...\n");

    while (1) {

        int client = accept(server, NULL, NULL);

        if (client < 0) {
            continue;
        }

        printf("Co client ket noi\n");

        pthread_t tid;

        int *pclient = malloc(sizeof(int));
        *pclient = client;

        pthread_create(&tid, NULL, client_thread, pclient);
    }

    close(server);

    return 0;
}

void *client_thread(void *arg) {

    int client = *(int *)arg;
    free(arg);

    char buf[1024];

    while (1) {

        int len = recv(client, buf, sizeof(buf) - 1, 0);

        if (len <= 0) {
            break;
        }

        buf[len] = 0;

        buf[strcspn(buf, "\r\n")] = 0;

        char command[100];
        char format[100];

        int n = sscanf(buf, "%s %s", command, format);

        if (n != 2 || strcmp(command, "GET_TIME") != 0) {

            send(client,
                 "Lenh khong hop le!\n",
                 strlen("Lenh khong hop le!\n"),
                 0);

            continue;
        }

        char time_format[50];

        if (strcmp(format, "dd/mm/yyyy") == 0) {
            strcpy(time_format, "%d/%m/%Y");
        }
        else if (strcmp(format, "dd/mm/yy") == 0) {
            strcpy(time_format, "%d/%m/%y");
        }
        else if (strcmp(format, "mm/dd/yyyy") == 0) {
            strcpy(time_format, "%m/%d/%Y");
        }
        else if (strcmp(format, "mm/dd/yy") == 0) {
            strcpy(time_format, "%m/%d/%y");
        }
        else {

            send(client,
                 "Format khong ho tro!\n",
                 strlen("Format khong ho tro!\n"),
                 0);

            continue;
        }

        time_t t = time(NULL);

        struct tm *tm_info = localtime(&t);

        char result[100];

        strftime(result, sizeof(result), time_format, tm_info);

        strcat(result, "\n");

        send(client, result, strlen(result), 0);
    }

    printf("Client da ngat ket noi\n");

    close(client);

    pthread_exit(NULL);
}
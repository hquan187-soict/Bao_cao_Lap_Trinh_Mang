#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define MAX_CLIENTS 100

typedef struct {
    int socket;
    char id[50];
    char name[50];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *client_thread(void *arg);

void broadcast(char *msg, int sender) {

    pthread_mutex_lock(&mutex);

    for (int i = 0; i < client_count; i++) {

        if (clients[i].socket != sender) {
            send(clients[i].socket, msg, strlen(msg), 0);
        }
    }

    pthread_mutex_unlock(&mutex);
}

void remove_client(int sock) {

    pthread_mutex_lock(&mutex);

    for (int i = 0; i < client_count; i++) {

        if (clients[i].socket == sock) {

            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }

            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&mutex);
}

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

    printf("Server dang lang nghe cong 8080...\n");

    while (1) {

        int client = accept(server, NULL, NULL);

        if (client < 0) {
            continue;
        }

        pthread_mutex_lock(&mutex);

        clients[client_count].socket = client;
        strcpy(clients[client_count].id, "");
        strcpy(clients[client_count].name, "");

        client_count++;

        pthread_mutex_unlock(&mutex);

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

    send(client, "Nhap theo cu phap: client_id: client_name\n",  strlen("Nhap theo cu phap: client_id: client_name\n"), 0);

    char client_id[50];
    char client_name[50];

    while (1) {

        int len = recv(client, buf, sizeof(buf) - 1, 0);

        if (len <= 0) {
            close(client);
            pthread_exit(NULL);
        }

        buf[len] = 0;

        if (sscanf(buf, "%[^:]: %s", client_id, client_name) == 2) {
            break;
        }

        send(client, "Sai cu phap. Nhap lai!\n", strlen("Sai cu phap. Nhap lai!\n"), 0);
    }

    pthread_mutex_lock(&mutex);

    for (int i = 0; i < client_count; i++) {

        if (clients[i].socket == client) {

            strcpy(clients[i].id, client_id);
            strcpy(clients[i].name, client_name);

            break;
        }
    }

    pthread_mutex_unlock(&mutex);

    printf("Client dang nhap: %s - %s\n", client_id, client_name);

    while (1) {

        int len = recv(client, buf, sizeof(buf) - 1, 0);

        if (len <= 0) {
            break;
        }

        buf[len] = 0;

        if (strcmp(buf, "exit\n") == 0) {
            break;
        }

        time_t t = time(NULL);

        struct tm *tm_info = localtime(&t);

        char timebuf[64];

        strftime(timebuf, sizeof(timebuf), "%Y/%m/%d %H:%M:%S", tm_info);

        char msg[1200];

        sprintf(msg, "%s %s: %s", timebuf, client_id, buf);

        printf("%s", msg);

        broadcast(msg, client);
    }

    printf("Client %s da thoat\n", client_id);

    remove_client(client);

    close(client);

    pthread_exit(NULL);
}
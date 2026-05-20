#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

int waiting_clients[MAX_CLIENTS];
int waiting_count = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int sender;
    int receiver;
} ChatPair;

void *forward_messages(void *arg) {
    ChatPair *pair = (ChatPair *)arg;

    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 50];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        memset(message, 0, sizeof(message));

        int len = recv(pair->sender, buffer, BUFFER_SIZE - 1, 0);

        if (len <= 0) {
            printf("Client %d disconnected\n", pair->sender);

            shutdown(pair->receiver, SHUT_RDWR);

            break;
        }

        buffer[len] = '\0';

        sprintf(message, "Client %d: %s", pair->sender, buffer);

        if (send(pair->receiver, message, strlen(message), 0) <= 0) {
            printf("Client %d disconnected\n", pair->receiver);

            shutdown(pair->sender, SHUT_RDWR);

            break;
        }
    }

    close(pair->sender);

    free(pair);

    return NULL;
}

void handle_pair(int c1, int c2) {
    pthread_t t1, t2;

    ChatPair *p1 = malloc(sizeof(ChatPair));
    ChatPair *p2 = malloc(sizeof(ChatPair));

    p1->sender = c1;
    p1->receiver = c2;

    p2->sender = c2;
    p2->receiver = c1;

    pthread_create(&t1, NULL, forward_messages, p1);
    pthread_create(&t2, NULL, forward_messages, p2);

    pthread_detach(t1);
    pthread_detach(t2);
}

int main() {
    int listener;

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

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
        close(listener);
        return 1;
    }

    if (listen(listener, 5) < 0) {
        perror("listen");
        close(listener);
        return 1;
    }

    printf("Chat server listening on port %d...\n", PORT);

    while (1) {

        int client = accept(listener, NULL, NULL);

        if (client < 0) {
            continue;
        }

        printf("New client connected: %d\n", client);

        pthread_mutex_lock(&mutex);

        waiting_clients[waiting_count++] = client;

        if (waiting_count >= 2) {

            int c1 = waiting_clients[0];
            int c2 = waiting_clients[1];

            for (int i = 2; i < waiting_count; i++) {
                waiting_clients[i - 2] = waiting_clients[i];
            }

            waiting_count -= 2;

            printf("Paired clients %d and %d\n", c1, c2);

            handle_pair(c1, c2);
        }

        pthread_mutex_unlock(&mutex);
    }

    close(listener);

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define THREAD_COUNT 5

int listener;

void *worker_thread(void *arg);

int main() {

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener == -1) {
        perror("socket()");
        return 1;
    }

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind()");
        close(listener);
        return 1;
    }

    if (listen(listener, 10)) {
        perror("listen()");
        close(listener);
        return 1;
    }

    printf("HTTP Server dang chay cong 8080...\n");

    pthread_t tids[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {

        pthread_create(&tids[i], NULL, worker_thread, NULL);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {

        pthread_join(tids[i], NULL);
    }

    close(listener);

    return 0;
}

void *worker_thread(void *arg) {

    while (1) {

        int client = accept(listener, NULL, NULL);

        if (client < 0) {
            continue;
        }

        printf("New client connected: %d\n", client);

        char buf[2048];

        int ret = recv(client, buf, sizeof(buf) - 1, 0);

        if (ret <= 0) {
            close(client);
            continue;
        }

        buf[ret] = 0;

        puts(buf);

        char *msg =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html>"
        "<body>"
        "<h1>Xin chao cac ban</h1>"
        "</body>"
        "</html>";

        send(client, msg, strlen(msg), 0);

        close(client);
    }

    pthread_exit(NULL);
}
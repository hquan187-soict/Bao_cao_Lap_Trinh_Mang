#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define NUM_WORKERS 4

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket() failed");
        return 1;
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        return 1;
    }

    if (listen(listener, 5) < 0) {
        perror("listen() failed");
        return 1;
    }

    printf("HTTP server started on port 8080 (Preforking with %d workers)\n", NUM_WORKERS);

    for (int i = 0; i < NUM_WORKERS; i++) {
        if (fork() == 0) {
            while (1) {
                int client = accept(listener, NULL, NULL);
                if (client < 0) continue;

                printf("Worker %d accepted client %d\n", getpid(), client);

                char buf[1024];
                int ret = recv(client, buf, sizeof(buf) - 1, 0);
                if (ret > 0) {
                    buf[ret] = 0;
                    puts(buf);
                }

                char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Xin chao cac ban</h1></body></html>";
                send(client, msg, strlen(msg), 0);

                close(client);
            }
            exit(0);
        }
    }

    for (int i = 0; i < NUM_WORKERS; i++) {
        wait(NULL);
    }

    close(listener);
    return 0;
}

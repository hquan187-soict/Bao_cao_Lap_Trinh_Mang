#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); exit(EXIT_FAILURE); }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(port);
    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Da ket noi toi %s:%d\n", argv[1], port);

    struct pollfd pfds[2];
    pfds[0].fd     = STDIN_FILENO;
    pfds[0].events = POLLIN;
    pfds[1].fd     = sock;
    pfds[1].events = POLLIN;

    char buf[BUFFER_SIZE];

    while (1) {
        int activity = poll(pfds, 2, -1);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        if (pfds[1].revents & POLLIN) {
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                printf("Server da dong ket noi.\n");
                break;
            }
            buf[n] = '\0';
            printf("%s", buf);
            fflush(stdout);
        }

        if (pfds[0].revents & POLLIN) {
            if (!fgets(buf, sizeof(buf), stdin)) break;
            send(sock, buf, strlen(buf), 0);
        }
    }

    close(sock);
    return 0;
}
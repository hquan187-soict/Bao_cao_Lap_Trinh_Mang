#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#define BUF 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <remote_ip> <remote_port>\n", argv[0]);
        return 1;
    }

    int port        = atoi(argv[1]);
    int remote_port = atoi(argv[3]);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in local = {0};
    local.sin_family      = AF_INET;
    local.sin_port        = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;
    bind(fd, (struct sockaddr *)&local, sizeof(local));

    struct sockaddr_in remote = {0};
    remote.sin_family = AF_INET;
    remote.sin_port   = htons(remote_port);
    inet_pton(AF_INET, argv[2], &remote.sin_addr);

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; fds[0].events = POLLIN;
    fds[1].fd = fd;           fds[1].events = POLLIN;

    char buf[BUF];

    while (1) {
        poll(fds, 2, -1);

        if (fds[0].revents & POLLIN) {
            if (!fgets(buf, sizeof(buf), stdin)) break;
            buf[strcspn(buf, "\n")] = '\0';
            if (strcmp(buf, "exit") == 0) break;
            sendto(fd, buf, strlen(buf), 0,
                   (struct sockaddr *)&remote, sizeof(remote));
        }

        if (fds[1].revents & POLLIN) {
            struct sockaddr_in src;
            socklen_t srclen = sizeof(src);
            int n = recvfrom(fd, buf, sizeof(buf)-1, 0,
                             (struct sockaddr *)&src, &srclen);
            if (n > 0) {
                buf[n] = '\0';
                printf("[%s:%d] %s\n",
                       inet_ntoa(src.sin_addr), ntohs(src.sin_port), buf);
            }
        }
    }

    close(fd);
    return 0;
}
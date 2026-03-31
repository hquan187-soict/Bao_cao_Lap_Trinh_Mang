#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define BUF_SIZE 512

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s port_s ip_d port_d\n", argv[0]);
        return 1;
    }

    int         port_s = atoi(argv[1]);
    const char *ip_d   = argv[2];
    int         port_d = atoi(argv[3]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1) {
        perror("socket() failed");
        return 1;
    }

    unsigned long ul = 1;
    ioctl(sockfd, FIONBIO, &ul);

    struct sockaddr_in local = {0};
    local.sin_family      = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port        = htons(port_s);

    if (bind(sockfd, (struct sockaddr *)&local, sizeof(local))) {
        perror("bind() failed");
        close(sockfd);
        return 1;
    }

    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(port_d);
    if (inet_pton(AF_INET, ip_d, &dest.sin_addr) <= 0) {
        fprintf(stderr, "IP khong hop le: %s\n", ip_d);
        close(sockfd);
        return 1;
    }

    printf("=== UDP Chat ===\n");
    printf("Lang nghe tren cong : %d\n", port_s);
    printf("Gui den             : %s:%d\n\n", ip_d, port_d);

    fd_set readfds;
    char buf[BUF_SIZE];
    char input[BUF_SIZE];

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        struct timeval tv = {0, 100000};
        select(sockfd + 1, &readfds, NULL, NULL, &tv);

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(input, 0, sizeof(input));
            if (fgets(input, sizeof(input), stdin) == NULL) break;
            sendto(sockfd, input, strlen(input), 0,
                   (struct sockaddr *)&dest, sizeof(dest));
        }

        if (FD_ISSET(sockfd, &readfds)) {
            memset(buf, 0, sizeof(buf));
            struct sockaddr_in sender = {0};
            socklen_t slen = sizeof(sender);
            int len = recvfrom(sockfd, buf, sizeof(buf) - 1, 0,
                               (struct sockaddr *)&sender, &slen);
            if (len > 0) {
                buf[len] = 0;
                printf("[%s:%d] %s",
                       inet_ntoa(sender.sin_addr),
                       ntohs(sender.sin_port),
                       buf);
                fflush(stdout);
            }
        }
    }

    close(sockfd);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <my_port> <peer_ip> <peer_port>\n", argv[0]);
        return 1;
    }

    int sockfd;
    struct sockaddr_in my_addr, peer_addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // bind port của mình
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(atoi(argv[1]));

    bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr));

    // địa chỉ peer
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(atoi(argv[3]));
    inet_pton(AF_INET, argv[2], &peer_addr.sin_addr);

    fd_set readfds;

    printf("UDP Chat started...\n");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);
        FD_SET(sockfd, &readfds);

        select(sockfd + 1, &readfds, NULL, NULL, NULL);
        if (FD_ISSET(0, &readfds)) {
            fgets(buffer, BUFFER_SIZE, stdin);
            sendto(sockfd, buffer, strlen(buffer), 0,
                (struct sockaddr*)&peer_addr, sizeof(peer_addr));
        }
        if (FD_ISSET(sockfd, &readfds)) {
            int len = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
            buffer[len] = '\0';
            printf("Peer: %s", buffer);
        }
    }

    close(sockfd);
    return 0;
}
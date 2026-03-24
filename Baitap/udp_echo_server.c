#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind"); close(sockfd); exit(1);
    }
    printf("UDP Echo Server dang chay tren port %s ...\n", argv[1]);

    char buf[BUF_SIZE];
    struct sockaddr_in cli_addr;
    socklen_t cli_len;

    while (1) {
        cli_len = sizeof(cli_addr);
        int n = recvfrom(sockfd, buf, BUF_SIZE, 0,
                         (struct sockaddr *)&cli_addr, &cli_len);
        if (n < 0) { perror("recvfrom"); continue; }

        buf[n] = '\0';
        char cli_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, cli_ip, sizeof(cli_ip));
        printf("Nhan tu %s:%d -> \"%s\" (%d bytes)\n",
               cli_ip, ntohs(cli_addr.sin_port), buf, n);

        sendto(sockfd, buf, n, 0,
               (struct sockaddr *)&cli_addr, cli_len);
    }

    close(sockfd);
    return 0;
}
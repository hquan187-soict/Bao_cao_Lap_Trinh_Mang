#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(1);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);

    printf("Nhap du lieu (Ctrl+D de thoat):\n");

    char send_buf[BUF_SIZE], recv_buf[BUF_SIZE];
    while (fgets(send_buf, BUF_SIZE, stdin) != NULL) {
        int len = strlen(send_buf);
        if (len > 0 && send_buf[len - 1] == '\n') {
            send_buf[len - 1] = '\0';
            len--;
        }
        if (len == 0) continue;

        sendto(sockfd, send_buf, len, 0,
               (struct sockaddr *)&serv_addr, sizeof(serv_addr));

        int n = recvfrom(sockfd, recv_buf, BUF_SIZE, 0, NULL, NULL);
        if (n < 0) { perror("recvfrom"); continue; }
        recv_buf[n] = '\0';

        printf("Echo: \"%s\" (%d bytes)\n", recv_buf, n);
    }

    close(sockfd);
    return 0;
}
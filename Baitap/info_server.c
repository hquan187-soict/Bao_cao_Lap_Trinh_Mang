#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF_SIZE 65536

/* Đọc chính xác n bytes từ socket */
int recv_all(int sockfd, unsigned char *buf, int n)
{
    int total = 0;
    while (total < n) {
        int r = recv(sockfd, buf + total, n - total, 0);
        if (r <= 0) return -1;
        total += r;
    }
    return total;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd, 5) < 0) {
        perror("listen");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
    if (connfd < 0) {
        perror("accept");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    char cli_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli_addr.sin_addr, cli_ip, sizeof(cli_ip));
    uint32_t total_len_n;
    if (recv_all(connfd, (unsigned char *)&total_len_n, 4) < 0) {
        fprintf(stderr, "Loi nhan du lieu\n");
        close(connfd);
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    uint32_t total_len = ntohl(total_len_n);

    unsigned char buf[BUF_SIZE];
    if (total_len > BUF_SIZE) {
        fprintf(stderr, "Du lieu qua lon\n");
        close(connfd);
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    if (recv_all(connfd, buf, (int)total_len) < 0) {
        fprintf(stderr, "Loi nhan payload\n");
        close(connfd);
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    int offset = 0;

    uint16_t dir_len_n;
    memcpy(&dir_len_n, buf + offset, 2);    offset += 2;
    uint16_t dir_len = ntohs(dir_len_n);

    char dir_name[1024];
    memcpy(dir_name, buf + offset, dir_len); offset += dir_len;
    dir_name[dir_len] = '\0';
    uint16_t fc_n;
    memcpy(&fc_n, buf + offset, 2);         offset += 2;
    uint16_t file_count = ntohs(fc_n);

    for (int i = 0; i < file_count; i++) {
        uint8_t name_len = buf[offset++];
        char filename[256];
        memcpy(filename, buf + offset, name_len); offset += name_len;
        filename[name_len] = '\0';

        uint32_t fsize_n;
        memcpy(&fsize_n, buf + offset, 4);  offset += 4;
        uint32_t fsize = ntohl(fsize_n);

        printf("%s : %u bytes\n", filename, fsize);
    }

    close(connfd);
    close(listenfd);
    return 0;
}
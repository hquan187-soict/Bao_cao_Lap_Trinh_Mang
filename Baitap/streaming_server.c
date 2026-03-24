#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024
#define PATTERN "0123456789"
#define PAT_LEN 10

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind"); close(listenfd); exit(1);
    }
    listen(listenfd, 5);
    printf("Dang lang nghe tren port %s ...\n", argv[1]);

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
    if (connfd < 0) { perror("accept"); close(listenfd); exit(1); }
    printf("Client da ket noi.\n");

    char tail[PAT_LEN]; 
    int tail_len = 0;
    int count = 0;
    int total_bytes = 0;

    char buf[BUF_SIZE];
    int n;

    while ((n = recv(connfd, buf, BUF_SIZE, 0)) > 0) {
        total_bytes += n;
        char tmp[PAT_LEN - 1 + BUF_SIZE];
        memcpy(tmp, tail, tail_len);
        memcpy(tmp + tail_len, buf, n);
        int tmp_len = tail_len + n;
        for (int i = 0; i <= tmp_len - PAT_LEN; i++) {
            if (memcmp(tmp + i, PATTERN, PAT_LEN) == 0)
                count++;
        }
        if (n >= PAT_LEN - 1) {
            memcpy(tail, buf + n - (PAT_LEN - 1), PAT_LEN - 1);
            tail_len = PAT_LEN - 1;
        } else {
            char combined[PAT_LEN * 2];
            memcpy(combined, tail, tail_len);
            memcpy(combined + tail_len, buf, n);
            int comb_len = tail_len + n;
            if (comb_len >= PAT_LEN - 1) {
                memcpy(tail, combined + comb_len - (PAT_LEN - 1), PAT_LEN - 1);
                tail_len = PAT_LEN - 1;
            } else {
                memcpy(tail, combined, comb_len);
                tail_len = comb_len;
            }
        }

        printf("Nhan %d bytes | Tong: %d bytes | So lan xuat hien \"%s\": %d\n", n, total_bytes, PATTERN, count);
    }

    printf("\n\"%s\" xuat hien %d lan (tong %d bytes)\n", PATTERN, count, total_bytes);

    close(connfd);
    close(listenfd);
    return 0;
}
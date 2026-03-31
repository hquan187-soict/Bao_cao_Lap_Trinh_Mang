#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT  8080
#define BUF_SIZE      1024

int main(int argc, char *argv[])
{
    const char *host = (argc >= 2) ? argv[1] : DEFAULT_HOST;
    int         port = (argc >= 3) ? atoi(argv[2]) : DEFAULT_PORT;

    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1) { perror("socket() failed"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Dia chi IP khong hop le: %s\n", host);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect() failed");
        return 1;
    }

    printf("[Client] Da ket noi toi %s:%d\n\n", host, port);

    char buf[BUF_SIZE];
    char input[BUF_SIZE];

    while (1) {
        memset(buf, 0, sizeof(buf));
        int n = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            printf("\n[Client] Ket noi den server da dong.\n");
            break;
        }
        printf("%s", buf);
        fflush(stdout);

        memset(input, 0, sizeof(input));
        if (fgets(input, sizeof(input), stdin) == NULL) break;

        send(sockfd, input, strlen(input), 0);

        if (strncasecmp(input, "quit", 4) == 0) {
            memset(buf, 0, sizeof(buf));
            recv(sockfd, buf, sizeof(buf) - 1, 0);
            printf("%s\n", buf);
            break;
        }
    }

    close(sockfd);
    return 0;
}
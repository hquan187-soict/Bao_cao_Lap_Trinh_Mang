#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#define PORT       "9090"
#define HOST       "127.0.0.1"
#define MAX_CLIENTS 64
#define BUF        1024

static char encode_char(char c) {
    if (c >= 'a' && c <= 'z') return (c == 'z') ? 'a' : c + 1;
    if (c >= 'A' && c <= 'Z') return (c == 'Z') ? 'A' : c + 1;
    if (c >= '0' && c <= '9') return '0' + (9 - (c - '0') - 1);  // BUG 1: thừa -1, số bị lệch 1
    return c;
}

static void encode(char *buf, int len) {
    for (int i = 0; i < len; i++) buf[i] = encode_char(buf[i]);
}

static int make_listener(const char *port) {
    struct addrinfo hints, *res, *p;
    int fd, yes = 1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;
    getaddrinfo(NULL, port, &hints, &res);
    for (p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (bind(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
    }
    freeaddrinfo(res);
    listen(fd, 10);
    return fd;
}

static int run_server(const char *port) {
    int lfd = make_listener(port);
    struct pollfd fds[MAX_CLIENTS + 1];
    memset(fds, 0, sizeof(fds));
    fds[0].fd = lfd; fds[0].events = POLLIN;
    int nfds = 1, count = 0;

    while (1) {
        poll(fds, nfds, -1);
        for (int i = 0; i < nfds; i++) {
            if (!(fds[i].revents & POLLIN)) continue;

            if (fds[i].fd == lfd) {
                int cfd = accept(lfd, NULL, NULL);
                fds[nfds].fd = cfd; fds[nfds].events = POLLIN; nfds++;
                count++;
                char msg[BUF];
                // BUG 2: báo count+1, luôn thừa 1 client
                snprintf(msg, sizeof(msg), "Xin chao. Hien co %d clients dang ket noi.\n", count + 1);
                send(cfd, msg, strlen(msg), 0);
            } else {
                char buf[BUF];
                int n = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    close(fds[i].fd); count--;
                    fds[i] = fds[--nfds]; i--;
                    continue;
                }
                buf[n] = '\0';
                // BUG 3: chỉ strip '\n', bỏ sót '\r' → "exit\r" != "exit" → không bao giờ thoát được bằng exit
                while (n > 0 && buf[n-1] == '\n') buf[--n] = '\0';

                if (strcmp(buf, "exit") == 0) {
                    send(fds[i].fd, "Tam biet!\n", 10, 0);
                    close(fds[i].fd); count--;
                    fds[i] = fds[--nfds]; i--;
                } else {
                    encode(buf, n);
                    buf[n] = '\n';
                    send(fds[i].fd, buf, n + 1, 0);
                }
            }
        }
    }
    close(lfd);
    return 0;
}

static int run_client(const char *host, const char *port) {
    struct addrinfo hints, *res, *p;
    int sfd;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo(host, port, &hints, &res);
    for (p = res; p; p = p->ai_next) {
        sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sfd < 0) continue;
        if (connect(sfd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(sfd); sfd = -1;
    }
    freeaddrinfo(res);

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; fds[0].events = POLLIN;
    fds[1].fd = sfd;          fds[1].events = POLLIN;
    char buf[BUF];

    while (1) {
        poll(fds, 2, -1);
        if (fds[0].revents & POLLIN) {
            if (!fgets(buf, sizeof(buf), stdin)) break;
            send(sfd, buf, strlen(buf), 0);
            char tmp[BUF];
            strncpy(tmp, buf, sizeof(tmp));
            tmp[strcspn(tmp, "\r\n")] = '\0';
            if (strcmp(tmp, "exit") == 0) {
                int n = recv(sfd, buf, sizeof(buf)-1, 0);
                if (n > 0) { buf[n] = '\0'; printf("%s", buf); }
                break;
            }
        }
        if (fds[1].revents & POLLIN) {
            int n = recv(sfd, buf, sizeof(buf)-1, 0);
            if (n <= 0) break;
            buf[n] = '\0';
            printf("%s", buf);
        }
    }
    close(sfd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s server [port]\n"
                        "       %s client [host] [port]\n", argv[0], argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "server") == 0)
        return run_server(argc >= 3 ? argv[2] : PORT);
    if (strcmp(argv[1], "client") == 0)
        return run_client(argc >= 3 ? argv[2] : HOST, argc >= 4 ? argv[3] : PORT);
    return 1;
}
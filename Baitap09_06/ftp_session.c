#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define HOST "lebavui.io.vn"
#define CTRL_PORT "21"
#define BUF_SIZE 4096

int tcp_connect(const char *host, const char *port) {
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        fprintf(stderr, "Loi getaddrinfo cho %s:%s\n", host, port);
        exit(1);
    }

    int fd = -1;
    for (p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == -1) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd == -1) {
        fprintf(stderr, "Khong the ket noi %s:%s\n", host, port);
        exit(1);
    }
    return fd;
}

void send_cmd(int sock, const char *cmd) {
    printf("CLIENT: %s", cmd);
    send(sock, cmd, strlen(cmd), 0);
}

int read_reply(int sock, char *out) {
    char buf[BUF_SIZE];
    int n = recv(sock, buf, BUF_SIZE - 1, 0);
    if (n <= 0) return -1;
    buf[n] = '\0';
    printf("SERVER: %s", buf);
    if (out) strcpy(out, buf);
    return n;
}

int open_passive_channel(int ctrl_sock) {
    char reply[BUF_SIZE];
    send_cmd(ctrl_sock, "PASV\r\n");
    read_reply(ctrl_sock, reply);

    int v[6];
    char *p = strchr(reply, '(');
    if (!p) {
        fprintf(stderr, "Khong parse duoc phan hoi PASV\n");
        exit(1);
    }
    sscanf(p, "(%d,%d,%d,%d,%d,%d)", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);

    char ip[32], port_str[8];
    snprintf(ip, sizeof(ip), "%d.%d.%d.%d", v[0], v[1], v[2], v[3]);
    snprintf(port_str, sizeof(port_str), "%d", v[4] * 256 + v[5]);

    return tcp_connect(ip, port_str);
}

int main(void) {
    char user[64], pass[64], cmd[256], reply[BUF_SIZE];

    printf("Username: ");
    scanf("%63s", user);
    printf("Password: ");
    scanf("%63s", pass);

    int ctrl = tcp_connect(HOST, CTRL_PORT);
    read_reply(ctrl, reply);

    snprintf(cmd, sizeof(cmd), "USER %s\r\n", user);
    send_cmd(ctrl, cmd);
    read_reply(ctrl, reply);

    snprintf(cmd, sizeof(cmd), "PASS %s\r\n", pass);
    send_cmd(ctrl, cmd);
    read_reply(ctrl, reply);

    send_cmd(ctrl, "TYPE I\r\n");
    read_reply(ctrl, reply);

    int data = open_passive_channel(ctrl);
    send_cmd(ctrl, "NLST\r\n");
    read_reply(ctrl, reply);

    char listing[BUF_SIZE] = {0};
    int n, total = 0;
    while ((n = recv(data, listing + total, BUF_SIZE - total - 1, 0)) > 0) {
        total += n;
    }
    listing[total] = '\0';
    close(data);
    read_reply(ctrl, reply);

    char qfile[128] = {0};
    char *tok = strtok(listing, "\r\n");
    while (tok) {
        if (strncmp(tok, "question_", 9) == 0) {
            strncpy(qfile, tok, sizeof(qfile) - 1);
            break;
        }
        tok = strtok(NULL, "\r\n");
    }
    if (qfile[0] == '\0') {
        printf("Khong tim thay file question_xxxxxx.txt\n");
        send_cmd(ctrl, "QUIT\r\n");
        close(ctrl);
        return 1;
    }
    printf("File cau hoi: %s\n", qfile);

    data = open_passive_channel(ctrl);
    snprintf(cmd, sizeof(cmd), "RETR %s\r\n", qfile);
    send_cmd(ctrl, cmd);
    read_reply(ctrl, reply);

    char content[128] = {0};
    total = 0;
    while ((n = recv(data, content + total, sizeof(content) - total - 1, 0)) > 0) {
        total += n;
    }
    content[total] = '\0';
    close(data);
    read_reply(ctrl, reply);
    printf("Noi dung goc (%d ky tu): %s\n", total, content);

    char reversed[128] = {0};
    int len = strlen(content);
    for (int i = 0; i < len; i++) {
        reversed[i] = content[len - 1 - i];
    }
    reversed[len] = '\0';

    char afile[128];
    const char *suffix = qfile + strlen("question_");
    snprintf(afile, sizeof(afile), "answer_%s", suffix);
    printf("File tra loi: %s\n", afile);
    printf("Noi dung dao: %s\n", reversed);

    data = open_passive_channel(ctrl);
    snprintf(cmd, sizeof(cmd), "STOR %s\r\n", afile);
    send_cmd(ctrl, cmd);
    read_reply(ctrl, reply);

    send(data, reversed, len, 0);
    close(data);
    read_reply(ctrl, reply);

    send_cmd(ctrl, "QUIT\r\n");
    read_reply(ctrl, reply);
    close(ctrl);

    return 0;
}
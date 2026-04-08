#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS     32
#define BUFFER_SIZE     1024
#define CMD_RESULT_FILE "out.txt"
#define MAX_LOGIN_TRY   3
#define BACKLOG         10

#define STATE_WAIT_USER  0
#define STATE_WAIT_PASS  1
#define STATE_LOGGED_IN  2

typedef struct {
    int  fd;
    int  state;
    char addr[INET_ADDRSTRLEN];
    char tmp_user[64];
    int  login_attempts;
} Client;

static Client clients[MAX_CLIENTS];
static int    client_count = 0;
static char   g_userdb[256];

static void trim_crlf(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\r' || s[len-1] == '\n' || s[len-1] == ' '))
        s[--len] = '\0';
}

static void send_str(int fd, const char *msg) {
    send(fd, msg, strlen(msg), 0);
}

static int find_client(int fd) {
    for (int i = 0; i < client_count; i++)
        if (clients[i].fd == fd) return i;
    return -1;
}

static void remove_client(int idx) {
    printf("Client %s (fd=%d) ngat ket noi.\n",
           clients[idx].addr, clients[idx].fd);
    close(clients[idx].fd);
    clients[idx] = clients[client_count - 1];
    client_count--;
}

static int add_client(int fd, const char *addr) {
    if (client_count >= MAX_CLIENTS) return -1;
    clients[client_count].fd             = fd;
    clients[client_count].state          = STATE_WAIT_USER;
    clients[client_count].login_attempts = 0;
    clients[client_count].tmp_user[0]    = '\0';
    strncpy(clients[client_count].addr, addr, INET_ADDRSTRLEN - 1);
    client_count++;
    return 0;
}

static int check_credentials(const char *username, const char *password) {
    FILE *fp = fopen(g_userdb, "r");
    if (!fp) return 0;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        trim_crlf(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        char db_user[64] = {0}, db_pass[64] = {0};
        if (sscanf(line, "%63s %63s", db_user, db_pass) < 2) continue;

        if (strcmp(db_user, username) == 0 && strcmp(db_pass, password) == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

static void execute_command(int fd, const char *cmd) {
    char shell_cmd[BUFFER_SIZE + 64];
    snprintf(shell_cmd, sizeof(shell_cmd), "%s > %s 2>&1", cmd, CMD_RESULT_FILE);

    printf("[CMD] fd=%d: %s\n", fd, shell_cmd);
    int ret = system(shell_cmd);

    FILE *fp = fopen(CMD_RESULT_FILE, "r");
    if (!fp) {
        send_str(fd, "[Loi: khong doc duoc ket qua lenh]\r\n$ ");
        return;
    }

    char buf[BUFFER_SIZE];
    int sent_any = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') {
            buf[len-1] = '\0';
            strncat(buf, "\r\n", sizeof(buf) - strlen(buf) - 1);
        }
        send_str(fd, buf);
        sent_any = 1;
    }
    fclose(fp);
    remove(CMD_RESULT_FILE);

    if (!sent_any) {
        if (ret == 0)
            send_str(fd, "[Lenh thanh cong, khong co output]\r\n");
        else
            send_str(fd, "[Lenh that bai hoac khong co output]\r\n");
    }

    send_str(fd, "$ ");
}

static void handle_client_data(int fd) {
    char buf[BUFFER_SIZE];
    int idx = find_client(fd);
    if (idx < 0) return;

    int nbytes = recv(fd, buf, sizeof(buf) - 1, 0);
    if (nbytes <= 0) {
        remove_client(idx);
        return;
    }

    buf[nbytes] = '\0';
    trim_crlf(buf);
    if (strlen(buf) == 0) return;

    if (clients[idx].state == STATE_WAIT_USER) {
        strncpy(clients[idx].tmp_user, buf, sizeof(clients[idx].tmp_user) - 1);
        send_str(fd, "Password: ");
        clients[idx].state = STATE_WAIT_PASS;
        return;
    }

    if (clients[idx].state == STATE_WAIT_PASS) {
        if (check_credentials(clients[idx].tmp_user, buf)) {
            clients[idx].state          = STATE_LOGGED_IN;
            clients[idx].login_attempts = 0;

            char welcome[256];
            snprintf(welcome, sizeof(welcome), "\r\nDang nhap thanh cong: %s.\r\n", clients[idx].tmp_user);
            send_str(fd, welcome);
            printf("Client %s dang nhap: user='%s'.\n",
                   clients[idx].addr, clients[idx].tmp_user);
        } else {
            clients[idx].login_attempts++;
            printf("Client %s dang nhap sai (user='%s', lan %d/%d).\n",
                   clients[idx].addr, clients[idx].tmp_user,
                   clients[idx].login_attempts, MAX_LOGIN_TRY);

            if (clients[idx].login_attempts >= MAX_LOGIN_TRY) {
                send_str(fd, "\r\nQua so lan thu! Ket noi bi dong.\r\n");
                remove_client(idx);
            } else {
                char errmsg[128];
                snprintf(errmsg, sizeof(errmsg), "\r\nSai ten dang nhap hoac mat khau! Con %d lan thu.\r\nUsername: ", MAX_LOGIN_TRY - clients[idx].login_attempts);
                send_str(fd, errmsg);
                clients[idx].state       = STATE_WAIT_USER;
                clients[idx].tmp_user[0] = '\0';
            }
        }
        return;
    }

    if (clients[idx].state == STATE_LOGGED_IN) {
        if (strcmp(buf, "exit") == 0 || strcmp(buf, "quit") == 0) {
            send_str(fd, "Goodbye!\r\n");
            remove_client(idx);
            return;
        }

        if (strncmp(buf, "rm ",  3) == 0 ||
            strncmp(buf, "dd ",  3) == 0 ||
            strncmp(buf, "mkfs", 4) == 0 ||
            strstr(buf, "> /") != NULL) {
            send_str(fd, "[Lenh bi tu choi vi ly do bao mat]\r\n$ ");
            return;
        }

        execute_command(fd, buf);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <userdb_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    strncpy(g_userdb, argv[2], sizeof(g_userdb) - 1);

    FILE *test = fopen(g_userdb, "r");
    if (!test) {
        fprintf(stderr, "Khong mo duoc file userdb '%s': %s\n",
                g_userdb, strerror(errno));
        exit(EXIT_FAILURE);
    }
    fclose(test);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    printf("Telnet Server lang nghe tren port %d\n", port);
    printf("Co so du lieu nguoi dung: %s\n\n", g_userdb);

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        for (int i = 0; i < client_count; i++) {
            FD_SET(clients[i].fd, &read_fds);
            if (clients[i].fd > max_fd)
                max_fd = clients[i].fd;
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in cli_addr;
            socklen_t addrlen = sizeof(cli_addr);
            int new_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &addrlen);
            if (new_fd < 0) {
                perror("accept");
            } else {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));

                if (add_client(new_fd, ip) < 0) {
                    send_str(new_fd, "Server day! Thu lai sau.\r\n");
                    close(new_fd);
                } else {
                    printf("Ket noi moi tu %s (fd=%d)\n", ip, new_fd);
                    send_str(new_fd,
                        "Username: ");
                }
            }
        }

        int snap_fds[MAX_CLIENTS];
        int snap_cnt = client_count;
        for (int i = 0; i < snap_cnt; i++)
            snap_fds[i] = clients[i].fd;

        for (int i = 0; i < snap_cnt; i++) {
            int fd = snap_fds[i];
            if (FD_ISSET(fd, &read_fds) && find_client(fd) >= 0)
                handle_client_data(fd);
        }
    }

    for (int i = 0; i < client_count; i++)
        close(clients[i].fd);
    close(server_fd);
    return 0;
}
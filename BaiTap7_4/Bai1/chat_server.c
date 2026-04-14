#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS     64
#define BUFFER_SIZE     1024
#define NAME_SIZE       64
#define ID_SIZE         32
#define BACKLOG         10

#define STATE_WAIT_ID   0
#define STATE_CHATTING  1

typedef struct {
    int  fd;
    int  state;
    char id[ID_SIZE];
    char name[NAME_SIZE];
    char addr[INET_ADDRSTRLEN];
} Client;

static Client      clients[MAX_CLIENTS];
static int         client_count = 0;
static struct pollfd pfds[MAX_CLIENTS + 1];

static void trim_crlf(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\r' || s[len-1] == '\n'))
        s[--len] = '\0';
}

static void get_timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, len, "%Y/%m/%d %H:%M:%S", t);
}

static void send_str(int fd, const char *msg) {
    send(fd, msg, strlen(msg), 0);
}

static int find_client(int fd) {
    for (int i = 0; i < client_count; i++)
        if (clients[i].fd == fd) return i;
    return -1;
}

static int find_client_by_id(const char *id) {
    for (int i = 0; i < client_count; i++)
        if (clients[i].state == STATE_CHATTING &&
            strcmp(clients[i].id, id) == 0)
            return i;
    return -1;
}

static void broadcast(int sender_fd, const char *msg) {
    for (int i = 0; i < client_count; i++)
        if (clients[i].fd != sender_fd && clients[i].state == STATE_CHATTING)
            send_str(clients[i].fd, msg);
}

static void rebuild_pfds(int server_fd) {
    pfds[0].fd     = server_fd;
    pfds[0].events = POLLIN;
    for (int i = 0; i < client_count; i++) {
        pfds[i + 1].fd     = clients[i].fd;
        pfds[i + 1].events = POLLIN;
    }
}

static void remove_client(int idx) {
    close(clients[idx].fd);
    clients[idx] = clients[client_count - 1];
    client_count--;
}

static int add_client(int fd, const char *addr) {
    if (client_count >= MAX_CLIENTS) return -1;
    clients[client_count].fd      = fd;
    clients[client_count].state   = STATE_WAIT_ID;
    clients[client_count].id[0]   = '\0';
    clients[client_count].name[0] = '\0';
    strncpy(clients[client_count].addr, addr, INET_ADDRSTRLEN - 1);
    client_count++;
    return 0;
}

static int parse_register(const char *line, char *out_id, char *out_name) {
    const char *sep = strstr(line, ": ");
    if (!sep) return 0;

    size_t id_len   = sep - line;
    size_t name_len = strlen(sep + 2);

    if (id_len == 0 || id_len >= ID_SIZE)      return 0;
    if (name_len == 0 || name_len >= NAME_SIZE) return 0;

    for (size_t i = 0; i < id_len; i++)
        if (line[i] == ' ') return 0;
    const char *name_ptr = sep + 2;
    for (size_t i = 0; i < name_len; i++)
        if (name_ptr[i] == ' ') return 0;

    strncpy(out_id,   line,     id_len);   out_id[id_len]     = '\0';
    strncpy(out_name, name_ptr, name_len); out_name[name_len] = '\0';
    return 1;
}

static void handle_client_data(int fd) {
    char buf[BUFFER_SIZE];
    int idx = find_client(fd);
    if (idx < 0) return;

    int nbytes = recv(fd, buf, sizeof(buf) - 1, 0);
    if (nbytes <= 0) {
        if (clients[idx].state == STATE_CHATTING) {
            char ts[32], notice[BUFFER_SIZE];
            get_timestamp(ts, sizeof(ts));
            snprintf(notice, sizeof(notice),
                     "[%s %s (%s) quited\n",
                     ts, clients[idx].name, clients[idx].id);
            broadcast(fd, notice);
            printf("%s' (%s) da ngat ket noi.\n",
                   clients[idx].id, clients[idx].addr);
        } else {
            printf("%s ngat ket noi (chua dang ky).\n",
                   clients[idx].addr);
        }
        remove_client(idx);
        return;
    }

    buf[nbytes] = '\0';
    trim_crlf(buf);
    if (strlen(buf) == 0) return;

    if (clients[idx].state == STATE_WAIT_ID) {
        char id[ID_SIZE], name[NAME_SIZE];
        if (!parse_register(buf, id, name)) {
            send_str(fd, "Cu phap sai! ");
            return;
        }
        if (find_client_by_id(id) >= 0) {
            send_str(fd, "ID nay da duoc su dung! Chon ID khac.\n> ");
            return;
        }
        strncpy(clients[idx].id,   id,   ID_SIZE - 1);
        strncpy(clients[idx].name, name, NAME_SIZE - 1);
        clients[idx].state = STATE_CHATTING;

        char welcome[BUFFER_SIZE];
        snprintf(welcome, sizeof(welcome),
                 "Chao mung %s (%s) da vao phong chat!\n", name, id);
        send_str(fd, welcome);

        char ts[32], notice[BUFFER_SIZE];
        get_timestamp(ts, sizeof(ts));
        snprintf(notice, sizeof(notice),
                 "[%s] %s (%s) da tham gia\n", ts, name, id);
        broadcast(fd, notice);
        printf("[SERVER] Client '%s' (%s) da dang ky tu %s.\n",
               id, name, clients[idx].addr);
        return;
    }

    if (clients[idx].state == STATE_CHATTING) {
        char ts[32], msg[BUFFER_SIZE + ID_SIZE + 40];
        get_timestamp(ts, sizeof(ts));
        snprintf(msg, sizeof(msg), "[%s] %s: %s\n", ts, clients[idx].id, buf);
        printf("[CHAT] %s", msg);
        broadcast(fd, msg);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

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

    printf("Port: %d\n", port);

    while (1) {
        rebuild_pfds(server_fd);

        int activity = poll(pfds, client_count + 1, -1);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        if (pfds[0].revents & POLLIN) {
            struct sockaddr_in cli_addr;
            socklen_t addrlen = sizeof(cli_addr);
            int new_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &addrlen);
            if (new_fd < 0) {
                perror("accept");
            } else {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
                if (add_client(new_fd, ip) < 0) {
                    send_str(new_fd, "Server day! Thu lai sau.\n");
                    close(new_fd);
                } else {
                    printf("Ket noi tu %s (fd=%d)\n", ip, new_fd);
                    send_str(new_fd,
                        "Dang ky theo cu phap: client_id: client_name\n");
                }
            }
        }

        int snap_fds[MAX_CLIENTS];
        int snap_cnt = client_count;
        for (int i = 0; i < snap_cnt; i++)
            snap_fds[i] = clients[i].fd;

        for (int i = 0; i < snap_cnt; i++) {
            if (pfds[i + 1].revents & POLLIN && find_client(snap_fds[i]) >= 0)
                handle_client_data(snap_fds[i]);
        }
    }

    for (int i = 0; i < client_count; i++)
        close(clients[i].fd);
    close(server_fd);
    return 0;
}
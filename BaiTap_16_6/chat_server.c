#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#define DEFAULT_PORT    9999
#define MAX_CLIENTS     64
#define BUF_SIZE        4096
#define NICK_LEN        64
#define TOPIC_LEN       256

#define RESP_OK             "100 OK\n"
#define RESP_NICK_IN_USE    "200 NICKNAME IN USE\n"
#define RESP_INVALID_NICK   "201 INVALID NICK NAME\n"
#define RESP_UNKNOWN_NICK   "202 UNKNOWN NICKNAME\n"
#define RESP_DENIED         "203 DENIED\n"
#define RESP_UNKNOWN_ERR    "999 UNKNOWN ERROR\n"

typedef struct {
    int     fd;
    char    nick[NICK_LEN];
    int     joined;
    pthread_t tid;
} Client;

static Client       clients[MAX_CLIENTS];
static int          client_count = 0;
static char         op_nick[NICK_LEN] = "";
static char         room_topic[TOPIC_LEN] = "No topic";
static pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;

static void send_to(int fd, const char *msg) {
    send(fd, msg, strlen(msg), 0);
}

static void broadcast(const char *msg, int exclude_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd > 0 && clients[i].joined) {
            if (clients[i].fd != exclude_fd) {
                send_to(clients[i].fd, msg);
            }
        }
    }
}

static int find_by_nick(const char *nick) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd > 0 && clients[i].joined &&
            strcasecmp(clients[i].nick, nick) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_empty_slot(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd <= 0) return i;
    }
    return -1;
}

static int is_valid_nick(const char *nick) {
    if (nick == NULL || nick[0] == '\0') return 0;
    for (int i = 0; nick[i]; i++) {
        if (!islower((unsigned char)nick[i]) && !isdigit((unsigned char)nick[i])) {
            return 0;
        }
    }
    return 1;
}

static int nick_in_use(const char *nick) {
    return find_by_nick(nick) != -1;
}

static void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r')) {
        s[--len] = '\0';
    }
}

static void handle_join(int slot, const char *args) {
    int fd = clients[slot].fd;

    if (args == NULL || args[0] == '\0') {
        send_to(fd, RESP_INVALID_NICK);
        return;
    }

    char nick[NICK_LEN];
    if (sscanf(args, "%63s", nick) != 1) {
        send_to(fd, RESP_INVALID_NICK);
        return;
    }

    if (!is_valid_nick(nick)) {
        send_to(fd, RESP_INVALID_NICK);
        return;
    }

    if (nick_in_use(nick)) {
        send_to(fd, RESP_NICK_IN_USE);
        return;
    }

    strncpy(clients[slot].nick, nick, NICK_LEN - 1);
    clients[slot].nick[NICK_LEN - 1] = '\0';
    clients[slot].joined = 1;
    client_count++;

    int is_first = (op_nick[0] == '\0');
    if (is_first) {
        strncpy(op_nick, nick, NICK_LEN - 1);
        op_nick[NICK_LEN - 1] = '\0';
    }

    send_to(fd, RESP_OK);

    char bcast[BUF_SIZE];
    snprintf(bcast, sizeof(bcast), "JOIN %s\n", nick);
    broadcast(bcast, fd);
}

static void handle_msg(int slot, const char *args) {
    int fd = clients[slot].fd;

    if (!clients[slot].joined) {
        send_to(fd, RESP_UNKNOWN_ERR);
        return;
    }
    if (args == NULL || args[0] == '\0') {
        send_to(fd, RESP_UNKNOWN_ERR);
        return;
    }

    send_to(fd, RESP_OK);

    char bcast[BUF_SIZE];
    snprintf(bcast, sizeof(bcast), "MSG %s %s\n", clients[slot].nick, args);
    broadcast(bcast, fd);
}

static void handle_pmsg(int slot, const char *args) {
    int fd = clients[slot].fd;

    if (!clients[slot].joined) {
        send_to(fd, RESP_UNKNOWN_ERR);
        return;
    }

    if (args == NULL || args[0] == '\0') {
        send_to(fd, RESP_UNKNOWN_NICK);
        return;
    }

    char target_nick[NICK_LEN];
    const char *msg_start;

    if (sscanf(args, "%63s", target_nick) != 1) {
        send_to(fd, RESP_UNKNOWN_NICK);
        return;
    }

    msg_start = args + strlen(target_nick);
    while (*msg_start == ' ') msg_start++;

    int target_slot = find_by_nick(target_nick);
    if (target_slot == -1) {
        send_to(fd, RESP_UNKNOWN_NICK);
        return;
    }

    send_to(fd, RESP_OK);

    char pmsg[BUF_SIZE];
    snprintf(pmsg, sizeof(pmsg), "PMSG %s %s\n", clients[slot].nick, msg_start);
    send_to(clients[target_slot].fd, pmsg);
}

static void handle_op(int slot, const char *args) {
    int fd = clients[slot].fd;

    if (!clients[slot].joined) {
        send_to(fd, RESP_UNKNOWN_ERR);
        return;
    }

    if (strcasecmp(clients[slot].nick, op_nick) != 0) {
        send_to(fd, RESP_DENIED);
        return;
    }

    if (args == NULL || args[0] == '\0') {
        send_to(fd, RESP_UNKNOWN_NICK);
        return;
    }

    char target_nick[NICK_LEN];
    if (sscanf(args, "%63s", target_nick) != 1) {
        send_to(fd, RESP_UNKNOWN_NICK);
        return;
    }

    int target_slot = find_by_nick(target_nick);
    if (target_slot == -1) {
        send_to(fd, RESP_UNKNOWN_NICK);
        return;
    }

    strncpy(op_nick, target_nick, NICK_LEN - 1);
    op_nick[NICK_LEN - 1] = '\0';

    send_to(fd, RESP_OK);

    char bcast[BUF_SIZE];
    snprintf(bcast, sizeof(bcast), "OP %s\n", target_nick);
    broadcast(bcast, fd);
}

static void handle_kick(int slot, const char *args) {
    int fd = clients[slot].fd;

    if (!clients[slot].joined) {
        send_to(fd, RESP_UNKNOWN_ERR);
        return;
    }

    if (strcasecmp(clients[slot].nick, op_nick) != 0) {
        send_to(fd, RESP_DENIED);
        return;
    }

    if (args == NULL || args[0] == '\0') {
        send_to(fd, RESP_UNKNOWN_NICK);
        return;
    }

    char target_nick[NICK_LEN];
    if (sscanf(args, "%63s", target_nick) != 1) {
        send_to(fd, RESP_UNKNOWN_NICK);
        return;
    }

    int target_slot = find_by_nick(target_nick);
    if (target_slot == -1) {
        send_to(fd, RESP_UNKNOWN_NICK);
        return;
    }

    if (target_slot == slot) {
        send_to(fd, RESP_DENIED);
        return;
    }

    send_to(fd, RESP_OK);

    char bcast[BUF_SIZE];
    snprintf(bcast, sizeof(bcast), "KICK %s %s\n", target_nick, clients[slot].nick);
    broadcast(bcast, fd);

    int kicked_fd = clients[target_slot].fd;
    clients[target_slot].fd = 0;
    clients[target_slot].joined = 0;
    clients[target_slot].nick[0] = '\0';
    client_count--;
    close(kicked_fd);
}

static void handle_topic(int slot, const char *args) {
    int fd = clients[slot].fd;

    if (!clients[slot].joined) {
        send_to(fd, RESP_UNKNOWN_ERR);
        return;
    }

    if (strcasecmp(clients[slot].nick, op_nick) != 0) {
        send_to(fd, RESP_DENIED);
        return;
    }

    if (args == NULL || args[0] == '\0') {
        send_to(fd, RESP_UNKNOWN_ERR);
        return;
    }

    strncpy(room_topic, args, TOPIC_LEN - 1);
    room_topic[TOPIC_LEN - 1] = '\0';

    send_to(fd, RESP_OK);

    char bcast[BUF_SIZE];
    snprintf(bcast, sizeof(bcast), "TOPIC %s %s\n", clients[slot].nick, room_topic);
    broadcast(bcast, fd);
}

static void handle_quit(int slot) {
    int fd = clients[slot].fd;
    char nick[NICK_LEN];
    strncpy(nick, clients[slot].nick, NICK_LEN);

    send_to(fd, RESP_OK);

    if (clients[slot].joined) {
        char bcast[BUF_SIZE];
        snprintf(bcast, sizeof(bcast), "QUIT %s\n", nick);
        broadcast(bcast, fd);

        if (strcasecmp(nick, op_nick) == 0) {
            op_nick[0] = '\0';
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd > 0 && clients[i].joined && i != slot) {
                    strncpy(op_nick, clients[i].nick, NICK_LEN - 1);
                    op_nick[NICK_LEN - 1] = '\0';
                    char op_msg[BUF_SIZE];
                    snprintf(op_msg, sizeof(op_msg), "OP %s\n", op_nick);
                    broadcast(op_msg, clients[slot].fd);
                    break;
                }
            }
        }

        clients[slot].joined = 0;
        client_count--;
    }

    clients[slot].fd = 0;
    clients[slot].nick[0] = '\0';
    close(fd);
}

static void dispatch_command(int slot, char *line) {
    trim_newline(line);

    char cmd[32] = {0};
    const char *args = NULL;

    char *space = strchr(line, ' ');
    if (space) {
        size_t cmd_len = (size_t)(space - line);
        if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
        strncpy(cmd, line, cmd_len);
        cmd[cmd_len] = '\0';
        args = space + 1;
        while (*args == ' ') args++;
    } else {
        strncpy(cmd, line, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
        args = "";
    }

    for (int i = 0; cmd[i]; i++) cmd[i] = toupper((unsigned char)cmd[i]);

    pthread_mutex_lock(&room_mutex);

    if (strcmp(cmd, "JOIN") == 0) {
        if (clients[slot].joined) {
            send_to(clients[slot].fd, RESP_UNKNOWN_ERR);
        } else {
            handle_join(slot, args);
        }
    } else if (!clients[slot].joined) {
        send_to(clients[slot].fd, RESP_UNKNOWN_ERR);
    } else if (strcmp(cmd, "MSG")   == 0) { handle_msg(slot, args);   }
    else if  (strcmp(cmd, "PMSG")  == 0) { handle_pmsg(slot, args);  }
    else if  (strcmp(cmd, "OP")    == 0) { handle_op(slot, args);    }
    else if  (strcmp(cmd, "KICK")  == 0) { handle_kick(slot, args);  }
    else if  (strcmp(cmd, "TOPIC") == 0) { handle_topic(slot, args); }
    else if  (strcmp(cmd, "QUIT")  == 0) { handle_quit(slot);        }
    else {
        send_to(clients[slot].fd, RESP_UNKNOWN_ERR);
    }

    pthread_mutex_unlock(&room_mutex);
}

static void *client_thread(void *arg) {
    int slot = *(int *)arg;
    free(arg);

    int fd = clients[slot].fd;
    char buf[BUF_SIZE];
    char line_buf[BUF_SIZE];
    int  line_len = 0;

    printf("[Server] Client connected: fd=%d, slot=%d\n", fd, slot);

    while (1) {
        int n = recv(fd, buf, sizeof(buf) - 1, 0);

        if (n <= 0) {
            pthread_mutex_lock(&room_mutex);
            if (clients[slot].joined) {
                char bcast[BUF_SIZE];
                snprintf(bcast, sizeof(bcast), "QUIT %s\n", clients[slot].nick);
                broadcast(bcast, fd);

                if (strcasecmp(clients[slot].nick, op_nick) == 0) {
                    op_nick[0] = '\0';
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i].fd > 0 && clients[i].joined && i != slot) {
                            strncpy(op_nick, clients[i].nick, NICK_LEN - 1);
                            op_nick[NICK_LEN - 1] = '\0';
                            char op_msg[BUF_SIZE];
                            snprintf(op_msg, sizeof(op_msg), "OP %s\n", op_nick);
                            broadcast(op_msg, fd);
                            break;
                        }
                    }
                }
                clients[slot].joined = 0;
                client_count--;
            }
            clients[slot].fd = 0;
            clients[slot].nick[0] = '\0';
            pthread_mutex_unlock(&room_mutex);
            close(fd);
            printf("[Server] Client disconnected: fd=%d, slot=%d\n", fd, slot);
            break;
        }

        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                line_buf[line_len] = '\0';
                if (line_len > 0) {
                    printf("[Server] From fd=%d: [%s]\n", fd, line_buf);
                    dispatch_command(slot, line_buf);
                }
                line_len = 0;
                if (clients[slot].fd == 0) goto client_done;
            } else if (c != '\r') {
                if (line_len < BUF_SIZE - 1) {
                    line_buf[line_len++] = c;
                }
            }
        }
    }

client_done:
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc >= 2) port = atoi(argv[1]);

    signal(SIGPIPE, SIG_IGN);

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(listener); return 1;
    }

    if (listen(listener, SOMAXCONN) < 0) {
        perror("listen"); close(listener); return 1;
    }

    printf("╔══════════════════════════════════════╗\n");
    printf("║  IT4060 Chat Server đang chạy        ║\n");
    printf("║  Port: %-5d  MaxClients: %-3d        ║\n", port, MAX_CLIENTS);
    printf("╚══════════════════════════════════════╝\n");

    memset(clients, 0, sizeof(clients));

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listener, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("[Server] New connection from %s:%d (fd=%d)\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port),
               client_fd);

        pthread_mutex_lock(&room_mutex);

        int slot = find_empty_slot();
        if (slot == -1) {
            pthread_mutex_unlock(&room_mutex);
            const char *err = "999 UNKNOWN ERROR\n";
            send(client_fd, err, strlen(err), 0);
            close(client_fd);
            printf("[Server] Room full, rejected fd=%d\n", client_fd);
            continue;
        }

        clients[slot].fd     = client_fd;
        clients[slot].joined = 0;
        clients[slot].nick[0] = '\0';

        pthread_mutex_unlock(&room_mutex);

        int *slot_ptr = malloc(sizeof(int));
        if (!slot_ptr) {
            close(client_fd);
            clients[slot].fd = 0;
            continue;
        }
        *slot_ptr = slot;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, slot_ptr) != 0) {
            perror("pthread_create");
            free(slot_ptr);
            close(client_fd);
            clients[slot].fd = 0;
        } else {
            clients[slot].tid = tid;
            pthread_detach(tid);
        }
    }

    close(listener);
    return 0;
}

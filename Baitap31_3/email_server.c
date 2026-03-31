#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define PORT        8080
#define MAX_CLIENTS 64
#define BUF_SIZE    256

typedef enum {
    STATE_ASK_NAME = 0,
    STATE_ASK_MSSV
} State;

typedef struct {
    int   fd;
    State state;
    char  name[BUF_SIZE];
} ClientInfo;

static void build_email(const char *fullname, const char *mssv,
                        char *out, int outsz)
{
    char tmp[BUF_SIZE];
    strncpy(tmp, fullname, BUF_SIZE - 1);
    tmp[BUF_SIZE - 1] = '\0';

    char words[16][64];
    int  wcount = 0;
    char *tok = strtok(tmp, " \t");
    while (tok && wcount < 16) {
        strncpy(words[wcount], tok, 63);
        words[wcount][63] = '\0';
        wcount++;
        tok = strtok(NULL, " \t");
    }

    if (wcount == 0) {
        snprintf(out, outsz, "unknown.%s@sis.hust.edu.vn", mssv);
        return;
    }

    char firstname[64];
    strncpy(firstname, words[wcount - 1], 63);
    firstname[0] = (char)toupper((unsigned char)firstname[0]);

    char initials[32] = "";
    for (int i = 0; i < wcount - 1; i++) {
        char c[2] = { (char)toupper((unsigned char)words[i][0]), '\0' };
        strncat(initials, c, sizeof(initials) - strlen(initials) - 1);
    }

    snprintf(out, outsz, "%s.%s%s@sis.hust.edu.vn", firstname, initials, mssv);
}

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("=== Server Email DHBK Ha Noi ===\n");
    printf("Dang lang nghe tren cong %d (non-blocking)...\n\n", PORT);

    ClientInfo clients[MAX_CLIENTS];
    int nclients = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd      = -1;
        clients[i].state   = STATE_ASK_NAME;
        clients[i].name[0] = '\0';
    }

    char buf[BUF_SIZE];
    int  len;

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client == -1) {
            if (errno != EWOULDBLOCK) {
                perror("accept() failed");
            }
        } else {
            printf("New client connected: %d\n", client);

            ul = 1;
            ioctl(client, FIONBIO, &ul);

            int placed = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == -1) {
                    clients[i].fd      = client;
                    clients[i].state   = STATE_ASK_NAME;
                    clients[i].name[0] = '\0';
                    if (i >= nclients) nclients = i + 1;
                    placed = 1;

                    const char *greet =
                        "Ho va ten: ";
                    send(client, greet, strlen(greet), 0);
                    break;
                }
            }
            if (!placed) {
                const char *full = "Server day, thu lai sau.\r\n";
                send(client, full, strlen(full), 0);
                close(client);
            }
        }

        for (int i = 0; i < nclients; i++) {
            if (clients[i].fd == -1) continue;

            memset(buf, 0, sizeof(buf));
            len = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);

            if (len == -1) {
                if (errno == EWOULDBLOCK) {
                    continue;
                } else {
                    printf("Client %d: loi, dong ket noi.\n", clients[i].fd);
                    close(clients[i].fd);
                    clients[i].fd    = -1;
                    clients[i].state = STATE_ASK_NAME;
                    continue;
                }
            }

            if (len == 0) {
                printf("Client %d da ngat ket noi.\n", clients[i].fd);
                close(clients[i].fd);
                clients[i].fd    = -1;
                clients[i].state = STATE_ASK_NAME;
                continue;
            }

            buf[len] = 0;
            buf[strcspn(buf, "\r\n")] = '\0';

            if (strcasecmp(buf, "quit") == 0) {
                const char *bye = "Tam biet!\r\n";
                send(clients[i].fd, bye, strlen(bye), 0);
                printf("Client %d da thoat.\n", clients[i].fd);
                close(clients[i].fd);
                clients[i].fd    = -1;
                clients[i].state = STATE_ASK_NAME;
                continue;
            }

            switch (clients[i].state) {

            case STATE_ASK_NAME:
                if (strlen(buf) == 0) {
                    const char *retry = "Ho ten khong duoc de trong. Vui long nhap lai: ";
                    send(clients[i].fd, retry, strlen(retry), 0);
                    break;
                }
                strncpy(clients[i].name, buf, BUF_SIZE - 1);
                printf("Client %d - Ho ten: \"%s\"\n", clients[i].fd, clients[i].name);
                {
                    const char *ask = "MSSV: ";
                    send(clients[i].fd, ask, strlen(ask), 0);
                }
                clients[i].state = STATE_ASK_MSSV;
                break;

            case STATE_ASK_MSSV:
                if (strlen(buf) == 0) {
                    const char *retry = "MSSV khong duoc de trong. Vui long nhap lai: ";
                    send(clients[i].fd, retry, strlen(retry), 0);
                    break;
                }
                printf("Client %d - MSSV: \"%s\"\n", clients[i].fd, buf);
                {
                    char email[BUF_SIZE];
                    build_email(clients[i].name, buf, email, sizeof(email));

                    char response[BUF_SIZE * 2];
                    snprintf(response, sizeof(response),
                             "\r\nDia chi email DHBKHN cua ban la: %s\r\n\r\n"
                             "Nhap Ho va ten de tra cuu tiep, hoac 'quit' de thoat: ",
                             email);
                    send(clients[i].fd, response, strlen(response), 0);
                    printf("Client %d -> Email: %s\n", clients[i].fd, email);
                }
                clients[i].state   = STATE_ASK_NAME;
                clients[i].name[0] = '\0';
                break;
            }
        }
    }

    close(listener);
    return 0;
}
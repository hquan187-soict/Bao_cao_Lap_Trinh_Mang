#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>

void signal_handler(int signo) {
    int stat;
    while (waitpid(-1, &stat, WNOHANG) > 0);
}

int check_login(const char *user, const char *pass) {
    FILE *f = fopen("users.txt", "r");
    if (!f) return 0;
    char file_user[256], file_pass[256];
    while (fscanf(f, "%s %s", file_user, file_pass) == 2) {
        if (strcmp(user, file_user) == 0 && strcmp(pass, file_pass) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket() failed");
        return 1;
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        return 1;
    }

    if (listen(listener, 5) < 0) {
        perror("listen() failed");
        return 1;
    }

    signal(SIGCHLD, signal_handler);
    printf("Telnet server started on port 9000\n");

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;

        printf("New client connected: %d\n", client);

        if (fork() == 0) {
            close(listener);
            
            char buf[1024];
            char user[256], pass[256];
            int logged_in = 0;

            char *msg = "Enter user and pass: \n";
            send(client, msg, strlen(msg), 0);

            int ret = recv(client, buf, sizeof(buf) - 1, 0);
            if (ret > 0) {
                buf[ret] = 0;
                if (sscanf(buf, "%s %s", user, pass) == 2) {
                    if (check_login(user, pass)) {
                        logged_in = 1;
                        char *ok_msg = "Login successful\n";
                        send(client, ok_msg, strlen(ok_msg), 0);
                    }
                }
            }

            if (!logged_in) {
                char *err_msg = "Login failed\n";
                send(client, err_msg, strlen(err_msg), 0);
                close(client);
                exit(0);
            }

            while (1) {
                ret = recv(client, buf, sizeof(buf) - 1, 0);
                if (ret <= 0) break;
                
                buf[ret] = 0;
                buf[strcspn(buf, "\r\n")] = 0; 
                
                if (strlen(buf) == 0) continue;

                char cmd[1050];
                sprintf(cmd, "%s > out.txt", buf);
                system(cmd);

                FILE *f = fopen("out.txt", "rb");
                if (f) {
                    char fbuf[1024];
                    int bytes_read;
                    while ((bytes_read = fread(fbuf, 1, sizeof(fbuf), f)) > 0) {
                        send(client, fbuf, bytes_read, 0);
                    }
                    fclose(f);
                } else {
                    char *err = "Command execution error\n";
                    send(client, err, strlen(err), 0);
                }
            }
            
            printf("Client %d disconnected\n", client);
            close(client);
            exit(0);
        }
        close(client);
    }

    close(listener);
    return 0;
}

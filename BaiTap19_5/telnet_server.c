#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

void *client_thread(void *arg);

int check_login(char *user, char *pass) {

    FILE *f = fopen("User.txt", "r");

    if (f == NULL) {
        return 0;
    }

    char db_user[100];
    char db_pass[100];

    while (fscanf(f, "%s %s", db_user, db_pass) != EOF) {

        if (strcmp(user, db_user) == 0 &&
            strcmp(pass, db_pass) == 0) {

            fclose(f);
            return 1;
        }
    }

    fclose(f);

    return 0;
}

int main() {

    int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server == -1) {
        perror("socket()");
        return 1;
    }

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind()");
        close(server);
        return 1;
    }

    if (listen(server, 10)) {
        perror("listen()");
        close(server);
        return 1;
    }

    printf("Telnet server dang chay cong 8080...\n");

    while (1) {

        int client = accept(server, NULL, NULL);

        if (client < 0) {
            continue;
        }

        printf("Co client ket noi\n");

        pthread_t tid;

        int *pclient = malloc(sizeof(int));
        *pclient = client;

        pthread_create(&tid, NULL, client_thread, pclient);
    }

    close(server);

    return 0;
}

void *client_thread(void *arg) {

    int client = *(int *)arg;
    free(arg);

    char buf[4096];

    send(client, "Username: ", strlen("Username: "), 0);

    int len = recv(client, buf, sizeof(buf) - 1, 0);

    if (len <= 0) {
        close(client);
        pthread_exit(NULL);
    }

    buf[len] = 0;
    buf[strcspn(buf, "\r\n")] = 0;

    char user[100];
    strcpy(user, buf);

    send(client, "Password: ", strlen("Password: "), 0);

    len = recv(client, buf, sizeof(buf) - 1, 0);

    if (len <= 0) {
        close(client);
        pthread_exit(NULL);
    }

    buf[len] = 0;
    buf[strcspn(buf, "\r\n")] = 0;

    char pass[100];
    strcpy(pass, buf);

    if (!check_login(user, pass)) {

        send(client, "Dang nhap that bai!\n", strlen("Dang nhap that bai!\n"), 0);

        close(client);

        pthread_exit(NULL);
    }

    send(client, "Dang nhap thanh cong!\n", strlen("Dang nhap thanh cong!\n"), 0);

    while (1) {

        send(client, "cmd> ", strlen("cmd> "), 0);

        len = recv(client, buf, sizeof(buf) - 1, 0);

        if (len <= 0) {
            break;
        }

        buf[len] = 0;
        buf[strcspn(buf, "\r\n")] = 0;

        if (strcmp(buf, "exit") == 0) {
            break;
        }

        char command[5000];

        sprintf(command, "%s > out.txt", buf);

        system(command);

        FILE *f = fopen("out.txt", "r");

        if (f == NULL) {

            send(client, "Khong mo duoc file out.txt\n", strlen("Khong mo duoc file out.txt\n"), 0);

            continue;
        }

        char output[4096];

        while (fgets(output, sizeof(output), f) != NULL) {

            send(client, output, strlen(output), 0);
        }

        fclose(f);

        send(client, "\n", 1, 0);
    }

    printf("Client da ngat ket noi\n");

    close(client);

    pthread_exit(NULL);
}
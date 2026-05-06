#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Loi: Khong the tao socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9090);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Loi: Ket noi toi server that bai");
        exit(EXIT_FAILURE);
    }

    printf(" Da ket noi toi Time Server thanh cong!\n");
    printf(" Cu phap yeu cau: GET_TIME [format]\n");
    printf("> Nhap lenh cua ban: ");

    char command[1024];
    if (fgets(command, sizeof(command), stdin) != NULL) {
        send(client_socket, command, strlen(command), 0);
        char response[1024];
        memset(response, 0, sizeof(response));
        int bytes_received = recv(client_socket, response, sizeof(response) - 1, 0);

        if (bytes_received > 0) {
            printf("\n[Server Tra Ve]: %s\n", response);
        } else {
            printf("\nLoi: Khong nhan duoc phan hoi tu server hoac ket noi bi dong.\n");
        }
    }

    close(client_socket);
    return 0;
}

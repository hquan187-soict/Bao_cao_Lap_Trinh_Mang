#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int port = atoi(argv[2]);
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("connect");
    exit(EXIT_FAILURE);
  }

  printf("Đã kết nối tới %s:%d\n", argv[1], port);

  char buf[BUFFER_SIZE];
  fd_set fds;

  while (1) {
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(sock, &fds);

    select(sock + 1, &fds, NULL, NULL, NULL);

    if (FD_ISSET(sock, &fds)) {
      int n = recv(sock, buf, sizeof(buf) - 1, 0);
      if (n <= 0) {
        printf("Đóng kết nối.\n");
        break;
      }
      buf[n] = '\0';
      printf("%s", buf);
      fflush(stdout);
    }

    if (FD_ISSET(STDIN_FILENO, &fds)) {
      if (!fgets(buf, sizeof(buf), stdin))
        break;
      send(sock, buf, strlen(buf), 0);
    }
  }

  close(sock);
  return 0;
}
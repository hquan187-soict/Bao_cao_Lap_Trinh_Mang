#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF_SIZE 65536

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    DIR *dp = opendir(".");
    if (!dp) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    typedef struct {
        char name[256];
        uint32_t size;
    } FileEntry;

    FileEntry files[1024];
    int file_count = 0;

    struct dirent *entry;
    struct stat st;
    while ((entry = readdir(dp)) != NULL) {
        if (stat(entry->d_name, &st) == 0 && S_ISREG(st.st_mode)) {
            strncpy(files[file_count].name, entry->d_name, 255);
            files[file_count].name[255] = '\0';
            files[file_count].size = (uint32_t)st.st_size;
            file_count++;
            if (file_count >= 1024) break;
        }
    }
    closedir(dp);

    printf("So file : %d\n", file_count);

    unsigned char buf[BUF_SIZE];
    int offset = 0;

    uint16_t dir_len = (uint16_t)strlen(cwd);
    uint16_t dir_len_n = htons(dir_len);
    memcpy(buf + offset, &dir_len_n, 2);   offset += 2;
    memcpy(buf + offset, cwd, dir_len);     offset += dir_len;

    uint16_t fc_n = htons((uint16_t)file_count);
    memcpy(buf + offset, &fc_n, 2);        offset += 2;

    for (int i = 0; i < file_count; i++) {
        uint8_t name_len = (uint8_t)strlen(files[i].name);
        buf[offset++] = name_len;

        memcpy(buf + offset, files[i].name, name_len);
        offset += name_len;

        uint32_t size_n = htonl(files[i].size);
        memcpy(buf + offset, &size_n, 4);
        offset += 4;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    uint32_t total_len = htonl((uint32_t)offset);
    send(sockfd, &total_len, 4, 0);
    send(sockfd, buf, offset, 0);

    printf("Da gui du lieu thanh cong!\n");

    close(sockfd);
    return 0;
}
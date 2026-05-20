#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

void handle_client(int client_sock, const char* dir_path) {
    DIR *dir;
    struct dirent *ent;
    int file_count = 0;
    
    dir = opendir(dir_path);
    if (dir == NULL) {
        char msg[] = "ERROR No files to download \r\n";
        send(client_sock, msg, strlen(msg), 0);
        close(client_sock);
        return;
    }
    
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG) {
            file_count++;
        }
    }
    closedir(dir);
    
    if (file_count == 0) {
        char msg[] = "ERROR No files to download \r\n";
        send(client_sock, msg, strlen(msg), 0);
        close(client_sock);
        return;
    }
    
    char header[256];
    sprintf(header, "OK %d\r\n", file_count);
    send(client_sock, header, strlen(header), 0);
    
    dir = opendir(dir_path);
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG) {
            send(client_sock, ent->d_name, strlen(ent->d_name), 0);
            send(client_sock, "\r\n", 2, 0);
        }
    }
    closedir(dir);
    send(client_sock, "\r\n", 2, 0);
    
    char buf[1024];
    while (1) {
        int n = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        
        while (n > 0 && (buf[n-1] == '\r' || buf[n-1] == '\n')) {
            buf[n-1] = '\0';
            n--;
        }
        if (n == 0) continue;
        
        char filepath[2048];
        sprintf(filepath, "%s/%s", dir_path, buf);
        
        FILE *f = fopen(filepath, "rb");
        if (f == NULL) {
            char err[] = "ERROR File not found\r\n";
            send(client_sock, err, strlen(err), 0);
        } else {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            sprintf(header, "OK %ld\r\n", fsize);
            send(client_sock, header, strlen(header), 0);
            
            char fbuf[4096];
            size_t bytes_read;
            while ((bytes_read = fread(fbuf, 1, sizeof(fbuf), f)) > 0) {
                send(client_sock, fbuf, bytes_read, 0);
            }
            fclose(f);
            break;
        }
    }
    close(client_sock);
}

void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        char usage[] = "Usage: ./server <port> <directory>\n";
        write(STDOUT_FILENO, usage, strlen(usage));
        return 1;
    }
    
    int port = atoi(argv[1]);
    char *dir_path = argv[2];
    
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);
    
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
    
    while (1) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock < 0) continue;
        
        if (fork() == 0) {
            close(server_sock);
            handle_client(client_sock, dir_path);
            exit(0);
        }
        close(client_sock);
    }
    
    return 0;
}

#include <arpa/inet.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 4096

void send_404(int client) {
    char *msg = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
    send(client, msg, strlen(msg), 0);
}
const char *get_mime_type(const char *path) {
    char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".c") == 0 ||
        strcmp(ext, ".cpp") == 0 || strcmp(ext, ".h") == 0 ||
        strcmp(ext, ".java") == 0 || strcmp(ext, ".py") == 0) return "text/plain";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    return "application/octet-stream";
}
void send_binary(int client, const char *path, const char *mime) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        send_404(client);
        return;
    }
    char header[1024];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nConnection: close\r\n\r\n", mime);
    send(client, header, strlen(header), 0);
    char buffer[BUFFER_SIZE];
    int n;
    while ((n = fread(buffer, 1, BUFFER_SIZE, f)) > 0) {
        send(client, buffer, n, 0);
    }
    fclose(f);
}
void send_media_page(int client, const char *url, const char *mime) {
    char html[8192];
    if (strncmp(mime, "image/", 6) == 0) {
        sprintf(html, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body style='background:black;margin:0;display:flex;justify-content:center;align-items:center;height:100vh;'><img src='%s/raw' style='max-width:95%%;max-height:95vh;'></body></html>", url);
        send(client, html, strlen(html), 0);
        return;
    }
    if (strncmp(mime, "video/", 6) == 0) {
        sprintf(html, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body style='background:black;margin:0;display:flex;justify-content:center;align-items:center;height:100vh;'><video controls autoplay style='width:90%%;max-height:95vh;'><source src='%s/raw'></video></body></html>", url);
        send(client, html, strlen(html), 0);
        return;
    }
    if (strncmp(mime, "audio/", 6) == 0) {
        sprintf(html, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body style='font-family:Arial;text-align:center;padding-top:100px;'><h2>Audio Player</h2><audio controls autoplay><source src='%s/raw'></audio></body></html>", url);
        send(client, html, strlen(html), 0);
        return;
    }
    sprintf(html, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body style='margin:0;'><iframe src='%s/raw' width='100%%' height='100%%' style='border:none;'></iframe></body></html>", url);
    send(client, html, strlen(html), 0);
}
void send_file(int client, const char *path, const char *url) {
    const char *mime = get_mime_type(path);
    if (strstr(url, "/raw") || strncmp(mime, "text/", 5) == 0) {
        send_binary(client, path, mime);
        return;
    }
    send_media_page(client, url, mime);
}
void send_directory(int client, const char *path, const char *url_path) {
    DIR *dir = opendir(path);
    if (!dir) {
        send_404(client);
        return;
    }
    char html[65536];
    strcpy(html, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><head><title>HTTP File Server</title></head><body style='font-family:Arial;'><h2>Directory Listing</h2><hr><ul>");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char fullpath[1024];
        sprintf(fullpath, "%s/%s", path, entry->d_name);
        struct stat st;
        stat(fullpath, &st);
        char line[2048];
        const char *base_url = (strcmp(url_path, "/") == 0) ? "" : url_path;
        if (S_ISDIR(st.st_mode)) {
            sprintf(line, "<li><b><a href='%s/%s'>%s</a></b></li>", base_url, entry->d_name, entry->d_name);
        } else {
            sprintf(line, "<li><i><a href='%s/%s'>%s</a></i></li>", base_url, entry->d_name, entry->d_name);
        }
        strcat(html, line);
    }
    strcat(html, "</ul></body></html>");
    send(client, html, strlen(html), 0);
    closedir(dir);
}
void handle_client(int client) {
    char buffer[BUFFER_SIZE];
    int n = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(client);
        return;
    }
    buffer[n] = '\0';
    printf("%s\n", buffer);
    char method[16], url[1024];
    sscanf(buffer, "%s %s", method, url);
    if (strcmp(method, "GET") != 0) {
        close(client);
        return;
    }
    char path[1024], real_path[1024];
    sprintf(path, ".%s", url);
    strcpy(real_path, path);
    char *p = strstr(real_path, "/raw");
    if (p) *p = '\0';
    struct stat st;
    if (stat(real_path, &st) < 0) {
        send_404(client);
        close(client);
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        send_directory(client, real_path, url);
    } else {
        send_file(client, real_path, url);
    }
    close(client);
}
int main() {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        perror("socket");
        return 1;
    }
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(server, 10) < 0) {
        perror("listen");
        return 1;
    }
    printf("HTTP File Server Running\n");
    printf("http://127.0.0.1:%d\n", PORT);
    while (1) {
        int client = accept(server, NULL, NULL);
        if (client < 0) continue;
        handle_client(client);
    }

    close(server);
    return 0;
}
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#define PORT 8080

double calculate(double a, double b, char *op){
    if (strcmp(op, "add") == 0) return a + b;
    if (strcmp(op, "sub") == 0) return a - b;
    if (strcmp(op, "mul") == 0)return a * b;
    if (strcmp(op, "div") == 0){
        if (b == 0) return 0;
        return a / b;
    }
    return 0;
}
char *get_operator_symbol(char *op) {
    if (strcmp(op, "add") == 0) return "+";
    if (strcmp(op, "sub") == 0) return "-";
    if (strcmp(op, "mul") == 0) return "*";
    if (strcmp(op, "div") == 0) return "/";
    return "?";
}
int main()
{
    int server = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, 5);
    printf("HTTP Server running at http://localhost:%d\n", PORT);
    while (1) {
        int client = accept(server, NULL, NULL);
        char buf[4096];
        int len = recv(client, buf, sizeof(buf) - 1, 0);
        if (len <= 0) {
            close(client);
            continue;
        }
        buf[len] = '\0';
        printf("%s\n", buf);
        double a = 0;
        double b = 0;
        char op[10] = "add";
        if (strncmp(buf, "GET /calc?", 10) == 0){
            sscanf(buf, "GET /calc?a=%lf&b=%lf&op=%s",&a, &b, op);
        }
        if (strncmp(buf, "POST /calc", 10) == 0)  {
            char *body = strstr(buf, "\r\n\r\n");
            if (body != NULL) {
                body += 4;
                sscanf(body, "a=%lf&b=%lf&op=%s", &a, &b, op);
            }
        }
        double result = calculate(a, b, op);
        char *symbol = get_operator_symbol(op);
        char html[4096];
        sprintf(html,
                "<html>"
                "<head>"
                "<title>Calculator</title>"
                "</head>"
                "<body>"
                "<h1>HTTP Calculator Server</h1>"
                "<h2>Ket qua</h2>"
                "<p>%lf %s %lf = %lf</p>"
                "<hr>"
                "<h2>Tinh bang GET</h2>"
                "<form method='GET' action='/calc'>"
                "So a:<br>"
                "<input type='text' name='a'><br><br>"
                "So b:<br>"
                "<input type='text' name='b'><br><br>"
                "Toan tu:<br>"
                "<select name='op'>"
                "<option value='add'>+</option>"
                "<option value='sub'>-</option>"
                "<option value='mul'>*</option>"
                "<option value='div'>/</option>"
                "</select>"
                "<br><br>"
                "<button type='submit'>Tinh GET</button>"
                "</form>"
                "<hr>"
                "<h2>Tinh bang POST</h2>"
                "<form method='POST' action='/calc'>"
                "So a:<br>"
                "<input type='text' name='a'><br><br>"
                "So b:<br>"
                "<input type='text' name='b'><br><br>"
                "Toan tu:<br>"
                "<select name='op'>"
                "<option value='add'>+</option>"
                "<option value='sub'>-</option>"
                "<option value='mul'>*</option>"
                "<option value='div'>/</option>"
                "</select>"
                "<br><br>"
                "<button type='submit'>Tinh POST</button>"
                "</form>"
                "</body>"
                "</html>",
                a, symbol, b, result);
        char response[8192];
        sprintf(response,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: %ld\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                strlen(html),
                html);
        send(client, response, strlen(response), 0);
        close(client);
    }
    close(server);
    return 0;
}

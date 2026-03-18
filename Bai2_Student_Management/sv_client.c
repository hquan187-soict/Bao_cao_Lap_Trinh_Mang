#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <arpa/inet.h>
 
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(1);
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);

    char student_id[20];
    char name[100];
    char dob[20];
    float gpa;

    printf("Enter student information:\n");
    printf("Student ID: ");
    scanf("%s", student_id);
    getchar();

    printf("Full name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';

    printf("Date of birth (YYYY-MM-DD): ");
    scanf("%s", dob);

    printf("GPA: ");
    scanf("%f", &gpa);

    char message[BUFFER_SIZE];
    snprintf(message, BUFFER_SIZE, "%s %s %s %.2f", student_id, name, dob, gpa);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(1);
    }

    printf("\nConnected to server. Sending student data...\n");

    if (send(sock, message, strlen(message), 0) < 0) {
        perror("Send failed");
        close(sock);
        exit(1);
    }

    printf("Data sent successfully!\n");

    close(sock);
    return 0;
}
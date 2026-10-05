#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    char *ip = "10.51.5.79";
    int port = 6767;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Socket creation error\n");
        return 1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Connection failed\n");
        return 1;
    }

    printf("Connected to %s:%d\n", ip, port);

    char buf[1024];
    while (1) {
        int bytes = read(sock, buf, sizeof(buf) - 1);
        if (bytes <= 0) {
            printf("Disconnected.\n");
            break;
        }

        buf[bytes] = '\0';
        printf("Received: %s", buf);

        if (strstr(buf, "Username?")) {
            write(sock, "PsychoX21", 9);
            printf("Sent: PsychoX21\n");
        }
        if (strstr(buf, "Password?")) {
            write(sock, "........", 8);
            printf("Sent: ........\n");
        }
        if (strstr(buf, "ping")) {
            write(sock, "pong\n", 5);
            printf("Sent: pong\n");
        }
    }

    close(sock);
    return 0;
}

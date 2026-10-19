#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common/protocol.h"

struct neighbour {
    char node;
    uint32_t ip;
    uint16_t port;
    int32_t cost;
};

char me;
int num_neighbours = 0;
struct neighbour neighbours[MAX_NODES];
int graph[MAX_NODES][MAX_NODES];

int read_full(int sockfd, void *buf, size_t len) {
    size_t bytes_read = 0;
    while (bytes_read < len) {
        ssize_t result = recv(sockfd, (char*)buf + bytes_read, len - bytes_read, 0);
        if (result < 0) {
            perror("Read Error");
            return -1;
        }
        if (result == 0) {
            break;
        }
        bytes_read += result;
    }
    return bytes_read;
}

void update_LS (int tcp_socket) {
    struct link_state_header header;
    if (read_full(tcp_socket, &header, sizeof(header)) != sizeof(header)) {
        perror("Read Header Error");
        return;
    }
    uint16_t num_records = ntohs(header.num_records);
    for (int i = 0; i < num_records; i++) {
        struct link_state_record record;
        if (read_full(tcp_socket, &record, sizeof(record)) != sizeof(record)) {
            perror("Read Record Error");
            return;
        }
        char node = record.node;
        uint32_t ip = ntohl(record.ip);
        uint16_t port = ntohs(record.port);
        int32_t cost = ntohl(record.cost);

        if (cost == 0 && me != node) {
            printf("Error: Multiple nodes with cost 0 detected. Current node: %c, New node: %c\n", me, node);
            continue;
        }

        if (cost == 0) {
            me = node;
        } else {
            neighbours[num_neighbours].node = node;
            neighbours[num_neighbours].ip = ip;
            neighbours[num_neighbours].port = port;
            neighbours[num_neighbours].cost = cost;
            num_neighbours++;
        }

        printf("Received link state record: node=%c, ip=%u, port=%u, cost=%d\n", node, ip, port, cost);
    }

    for (int i = 0; i < num_neighbours; i++) {
        graph[me - 'A'][neighbours[i].node - 'A'] = neighbours[i].cost;
        graph[neighbours[i].node - 'A'][me - 'A'] = neighbours[i].cost;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <oracle_ip> <myudp_port> [<myip>]\n", argv[0]);
        return 1;
    }

    char* oracle_ip = argv[1];
    int oracle_port = ORACLE_PORT;
    int myudp_port = atoi(argv[2]);
    char* myip = (argc > 3) ? argv[3] : "127.0.0.1";

    for (int i = 0; i < MAX_NODES; i++) {
        for (int j = 0; j < MAX_NODES; j++) {
            graph[i][j] = (i == j) ? 0 : -1;
        }
    }

    int udp_socket;
    struct sockaddr_in myaddr;
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("UDP Socket Error");
        return 1;
    }
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons(myudp_port);
    myaddr.sin_addr.s_addr = inet_addr(myip);
    if (bind(udp_socket, (struct sockaddr*)&myaddr, sizeof(myaddr)) < 0) {
        perror("UDP Bind Error");
        close(udp_socket);
        return 1;
    }

    int oracle_socket;
    struct sockaddr_in oracle_addr;
    if ((oracle_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Oracle Socket Error");
        close(udp_socket);
        return 1;
    }
    memset(&oracle_addr, 0, sizeof(oracle_addr));
    oracle_addr.sin_family = AF_INET;
    oracle_addr.sin_port = htons(oracle_port);
    oracle_addr.sin_addr.s_addr = inet_addr(oracle_ip);
    if (connect(oracle_socket, (struct sockaddr*)&oracle_addr, sizeof(oracle_addr)) < 0) {
        perror("Oracle Connect Error");
        close(udp_socket);
        close(oracle_socket);
        return 1;
    }

    struct connect_msg msg;
    msg.ip = inet_addr(myip);
    msg.port = htons(myudp_port);
    send(oracle_socket, &msg, sizeof(msg), 0);
    printf("Sent connection message to oracle: ip=%s, port=%d\n", myip, myudp_port);
    update_LS(oracle_socket);
}


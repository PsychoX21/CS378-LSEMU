#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common/protocol.h"

#define SENDBLOCKING 0

struct neighbour {
    char node;
    uint32_t ip;
    uint16_t port;
    int32_t cost;
};

char me;
int num_neighbours = 0;
int total_nodes = 1;
struct neighbour neighbours[MAX_NODES];
int graph[MAX_NODES][MAX_NODES];

uint32_t seqmax[MAX_NODES] = {0};

int read_full(int sockfd, void *buf, size_t len) {
    size_t bytes_read = 0;
    while (bytes_read < len) {
        ssize_t result = recv(sockfd, (char*)buf + bytes_read, len - bytes_read, 0);
        if (result < 0) {
            fprintf(stderr, "Read Error\n");
            return -1;
        }
        if (result == 0) {
            break;
        }
        bytes_read += result;
    }
    return bytes_read;
}

int send_LSP(int udp_socket)
{
    seqmax[me - 'A']++;
    int offset = 0;

    char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);

    struct lsp_header header;
    header.origin = me;
    header.seq = htonl(seqmax[me - 'A']);
    header.num_neighbors = htons(num_neighbours);
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    for (int i = 0; i < num_neighbours; i++)
    {
        struct lsp_neighbor neighbor;
        neighbor.neighbor = neighbours[i].node;
        neighbor.cost = htonl(neighbours[i].cost);
        memcpy(buffer + offset, &neighbor, sizeof(neighbor));
        offset += sizeof(neighbor);
    }

    for (int i = 0; i < num_neighbours; i++)
    {
        struct sockaddr_in neighbor_addr;
        memset(&neighbor_addr, 0, sizeof(neighbor_addr));
        neighbor_addr.sin_family = AF_INET;
        neighbor_addr.sin_port = htons(neighbours[i].port);
        neighbor_addr.sin_addr.s_addr = htonl(neighbours[i].ip);

        if (sendto(udp_socket, buffer, offset, 0, (struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr)) < 0)
        {
            return -1;
        }
    }

    return 0;
}

int forward_LSP(int udp_socket, char *buffer, int length, struct sockaddr_in sender_addr) {
    for (int i = 0; i < num_neighbours; i++)
    {
        if (sender_addr.sin_addr.s_addr == htonl(neighbours[i].ip) && sender_addr.sin_port == htons(neighbours[i].port))
        {
            continue;
        }
        struct sockaddr_in neighbor_addr;
        memset(&neighbor_addr, 0, sizeof(neighbor_addr));
        neighbor_addr.sin_family = AF_INET;
        neighbor_addr.sin_addr.s_addr = htonl(neighbours[i].ip);
        neighbor_addr.sin_port = htons(neighbours[i].port);

        if (sendto(udp_socket, buffer, length, 0, (struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr)) < 0)
        {
            fprintf(stderr, "Forward LSP Error\n");
            return -1;
        }
    }
    return 0;
}

int receive_LSP(int udp_socket)
{
    char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);
    int offset = 0;

    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
    ssize_t bytes_received = recvfrom(udp_socket, buffer, BUF_SIZE, 0, (struct sockaddr *)&sender_addr, &sender_addr_len);

    struct lsp_header header;

    if ((long unsigned int) bytes_received < sizeof(header))
    {
        fprintf(stderr, "Receive LSP Error\n");
        return -1;
    }

    memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);
    char origin = header.origin;
    uint32_t seq = ntohl(header.seq);
    uint16_t num_neighbors = ntohs(header.num_neighbors);

    int expected_size = sizeof(header) + num_neighbors * sizeof(struct lsp_neighbor);
    if ((int) bytes_received < expected_size)
    {
        fprintf(stderr, "Incomplete LSP received: expected %d bytes, got %ld bytes\n", expected_size, bytes_received);
        return -1;
    }

    if (origin - 'A' >= MAX_NODES || origin - 'A' < 0)
    {
        fprintf(stderr, "Invalid origin node: %c\n", origin);
        return -1;
    }

    if (origin - 'A' + 1 > total_nodes)
    {
        total_nodes = origin - 'A' + 1;
    }

    if (seq <= seqmax[origin - 'A'])
        return 0;
    seqmax[origin - 'A'] = seq;

    for (int i = 0; i < MAX_NODES; i++)
    {
        graph[origin - 'A'][i] = (i == origin - 'A') ? 0 : -1;
        graph[i][origin - 'A'] = (i == origin - 'A') ? 0 : -1;
    }

    for (int i = 0; i < num_neighbors; i++)
    {
        struct lsp_neighbor neighbor;
        memcpy(&neighbor, buffer + offset, sizeof(neighbor));
        offset += sizeof(neighbor);
        char neighbor_node = neighbor.neighbor;
        int32_t cost = ntohl(neighbor.cost);

        if (neighbor_node - 'A' >= MAX_NODES || neighbor_node - 'A' < 0)
        {
            fprintf(stderr, "Invalid neighbor node: %c\n", neighbor_node);
            continue;
        }

        graph[origin - 'A'][neighbor_node - 'A'] = cost;
        graph[neighbor_node - 'A'][origin - 'A'] = cost;

        if (neighbor_node - 'A' + 1 > total_nodes)
        {
            total_nodes = neighbor_node - 'A' + 1;
        }
    }

    printf("LSP sequence number from %c: %u\n", origin, seq);
    if (forward_LSP(udp_socket, buffer, offset, sender_addr) < 0)
    {
        fprintf(stderr, "Error forwarding LSP\n");
        return -1;
    }
    return 0;
}

int update_LS (int tcp_socket, int udp_socket) {
    struct link_state_header header;
    if (read_full(tcp_socket, &header, sizeof(header)) != sizeof(header)) {
        fprintf(stderr, "Read LS Header Error\n");
        return -1;
    }
    num_neighbours = 0;
    uint16_t num_records = ntohs(header.num_records);
    for (int i = 0; i < num_records; i++) {
        struct link_state_record record;
        if (read_full(tcp_socket, &record, sizeof(record)) != sizeof(record)) {
            fprintf(stderr, "Read LS Record Error\n");
            return -1;
        }
        char node = record.node;
        uint32_t ip = ntohl(record.ip);
        uint16_t port = ntohs(record.port);
        int32_t cost = ntohl(record.cost);

        if (node - 'A' + 1 > total_nodes) {
            total_nodes = node - 'A' + 1;
        }

        if (cost == 0 && me != 0 && me != node) {
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
    }

    for (int i = 0; i < MAX_NODES; i++) {
        graph[me - 'A'][i] = (i == me - 'A') ? 0 : -1;
        graph[i][me - 'A'] = (i == me - 'A') ? 0 : -1;
    }
    for (int i = 0; i < num_neighbours; i++) {
        graph[me - 'A'][neighbours[i].node - 'A'] = neighbours[i].cost;
        graph[neighbours[i].node - 'A'][me - 'A'] = neighbours[i].cost;
    }

    // Print neighbours
    printf("%c=0", me);
    for (int i = 0; i < num_neighbours; i++) {
        printf(",%c=%d", neighbours[i].node, neighbours[i].cost);
    }
    printf("\n");

    if (send_LSP(udp_socket) < 0) {
        fprintf(stderr, "Error sending LSP\n");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <oracle_ip> <myudp_port> [<myip>]\n", argv[0]);
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering for stdout

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
        fprintf(stderr, "UDP Socket Error\n");
        return 1;
    }
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons(myudp_port);
    myaddr.sin_addr.s_addr = inet_addr(myip);
    if (bind(udp_socket, (struct sockaddr*)&myaddr, sizeof(myaddr)) < 0) {
        fprintf(stderr, "UDP Bind Error\n");
        close(udp_socket);
        return 1;
    }

    int oracle_socket;
    struct sockaddr_in oracle_addr;
    if ((oracle_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Oracle Socket Error\n");
        close(udp_socket);
        return 1;
    }
    memset(&oracle_addr, 0, sizeof(oracle_addr));
    oracle_addr.sin_family = AF_INET;
    oracle_addr.sin_port = htons(oracle_port);
    oracle_addr.sin_addr.s_addr = inet_addr(oracle_ip);
    if (connect(oracle_socket, (struct sockaddr*)&oracle_addr, sizeof(oracle_addr)) < 0) {
        fprintf(stderr, "Oracle Connect Error\n");
        close(udp_socket);
        close(oracle_socket);
        return 1;
    }

    struct connect_msg msg;
    msg.ip = inet_addr(myip);
    msg.port = htons(myudp_port);

    while(SENDBLOCKING) {
        static int count = 0;
        printf("Sending connection message to oracle (attempt %d)...\n", ++count);
        printf("This proves that ");
        fflush(stdout);
        send(oracle_socket, &msg, sizeof(msg), 0);
        printf("Send is Blocking\n");
    }

    send(oracle_socket, &msg, sizeof(msg), 0);
    printf("Sent connection message to oracle\n");
    if (update_LS(oracle_socket, udp_socket) < 0) {
        fprintf(stderr, "Error creating Link State\n");
        close(udp_socket);
        close(oracle_socket);
        return 1;
    }

    time_t last_update_time = time(NULL);

    while(1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(udp_socket, &readfds);
        FD_SET(oracle_socket, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int max_fd = (udp_socket > oracle_socket) ? udp_socket : oracle_socket;

        if (select(max_fd + 1, &readfds, NULL, NULL, &timeout) < 0) {
            fprintf(stderr, "Select Error\n");
            close(udp_socket);
            close(oracle_socket);
            return 1;
        }

        if (FD_ISSET(udp_socket, &readfds)) {
            if (receive_LSP(udp_socket) < 0) {
                fprintf(stderr, "Error receiving LSP\n");
                close(udp_socket);
                close(oracle_socket);
                return 1;
            }
        }

        if (FD_ISSET(oracle_socket, &readfds)) {
            if (update_LS(oracle_socket, udp_socket) < 0) {
                fprintf(stderr, "Error updating Link State\n");
                close(udp_socket);
                close(oracle_socket);
                return 1;
            }
            last_update_time = time(NULL);
        }

        time_t current_time = time(NULL);
        if (current_time - last_update_time >= PERIODIC_UPDATE_INTERVAL) {
            printf("Periodic update: Sending LSP...\n");
            if (send_LSP(udp_socket) < 0) {
                fprintf(stderr, "Error sending LSP\n");
                close(udp_socket);
                close(oracle_socket);
                return 1;
            }
            printf("Current Network Topology:\n");
            for (int i = 0; i < total_nodes; i++) {
                for (int j = 0; j < total_nodes; j++) {
                    if (graph[i][j] == -1) {
                        printf("INF ");
                    } else {
                        printf("%d ", graph[i][j]);
                    }
                }
                printf("\n");
            }
            printf("\n");
            last_update_time = current_time;
        }
    }

    close(udp_socket);
    close(oracle_socket);
    return 0;
}


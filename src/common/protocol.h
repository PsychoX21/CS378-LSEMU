#include <stdint.h>
#include <arpa/inet.h>

#define ORACLE_PORT 5000
#define MAX_NODES 26
#define BUF_SIZE 2048
#define PERIODIC_UPDATE_INTERVAL 10

struct connect_msg {
    uint32_t ip;
    uint16_t port;
} __attribute__((packed));

struct link_state_header {
    uint16_t num_records;
} __attribute__((packed));

struct link_state_record {
    char node;
    uint32_t ip;
    uint16_t port;
    int32_t cost;
} __attribute__((packed));

struct lsp_header {
    char origin;
    uint32_t seq;
    uint16_t num_neighbors;
} __attribute__((packed));

struct lsp_neighbor {
    char neighbor;
    int32_t cost;
} __attribute__((packed));

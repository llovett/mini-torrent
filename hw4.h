struct peer_addr {
    in_addr_t addr;
    short port;
} __attribute__((packed));

struct peer_state {
    struct peer_state *next;
    in_addr_t ip;

    int socket;
    int connected;
    char* bitfield;
    char* incoming; // buffer where we store partial messages
    int requested_piece;

    int count; // number of bytes currently in the incoming buffer
    int choked;
};

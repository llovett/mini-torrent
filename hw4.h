struct peer_addr {
    in_addr_t addr;
    short port;
} __attribute__((packed));

struct peer_state {
    struct peer_state *next;
    in_addr_t ip;

    int socket;
    int connected;
    int rcv_handshake;
    char* bitfield;
    char* incoming; // buffer where we store partial messages
    char *outgoing;
    int outgoing_count;
    int requested_piece;

    int count; // number of bytes currently in the incoming buffer
    int choked;
};

// Function defs
void print_bencode(struct bencode*);
void start_peers();
void buffer_message(struct peer_state *peer, const void *msg, int len);
void shutdown_peer(struct peer_state *peer);

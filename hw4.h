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
    char *outgoing; // buffer for messages going out to this peer
    int outgoing_count; // number of bytes to be sent to this peer
    int requested_piece;

    int count; // number of bytes currently in the incoming buffer
    int choked;
};

// function defs
int send_message(struct peer_state *peer, const void *msg, int len);
void print_bencode(struct bencode*);
void start_peers(void);
void handle_announcement(char *ptr, size_t size);

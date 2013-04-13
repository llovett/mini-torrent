#include<stdlib.h>
#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<sys/types.h>
#include<sys/socket.h>
#include <openssl/sha.h>
#include<bencodetools/bencode.h>
#include<curl/curl.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include"hw4.h"

#define PRINT(x) if(debug){puts(x);fflush(stdout);}

// read/write sets
fd_set readset, writeset;

// computed in main(). This is where we get our peers from.
char announce_url[255];

#define BUFSIZE piece_length*2-1
struct peer_state *peers=0;

int debug=1;  //set this to zero if you don't want all the debugging messages

char screenbuf[10000];

// The SHA1 digest of the document we're downloading this time.
// using a global means we can only ever download a single torrent in a single process. That's ok.
unsigned char digest[20];

// we generate a new random peer id every time we start
char peer_id[21]="-OBCS342-";
struct bencode_dict *torrent;
struct bencode *info;

int file_length=0;
int piece_length;

int active_peers() {
    struct peer_state *peer = peers;
    int count=0;
    while(peer) {
	if(peer->connected && !peer->choked)
	    count++;
	peer = peer->next;
    }
    return count;
}

int choked_peers() {
    struct peer_state *peer = peers;
    int count=0;
    while(peer) {
	if(peer->connected && peer->choked)
	    count++;
	peer = peer->next;
    }
    return count;
}

int peer_connected(in_addr_t addr, int port) {
    struct peer_state *peer = peers;
    while(peer) {
	if(peer->ip == addr && peer->port == port && peer->connected==1) {
	    return 1;
	}
	peer = peer->next;
    }
    return 0;
}

void* draw_state() {
    printf("\033[2J\033[1;1H");
    int pieces = file_length/piece_length+1;

    printf("%d byte file, %d byte pieces, %d pieces, %d active, %d choked\n",file_length,piece_length,file_length/piece_length+1,active_peers(),choked_peers());
    for(int i=0;i<pieces;i++) {
	if(i%80 == 0) printf("\n");

	switch(piece_status[i].status) {
	case PIECE_EMPTY: printf("."); break;
	case PIECE_PENDING: printf("x"); break;
	case PIECE_FINISHED: printf("X"); break;
	default: printf("?");
	}
    }
    fflush(stdout);
}


int missing_blocks() {
    int count=0;

    for(int i=0;i<file_length/piece_length+((file_length%piece_length)>0?1:0);i++) {
	if(piece_status[i].status!=PIECE_FINISHED) {
	    count++;
	}
    }
    return count;
}

/**
 * checks to see if the <peer> has piece number <piece>
 * */
int has_piece(struct peer_state *peer, int piece) {
    return ((peer->bitfield[piece/8]>>(7-(piece%8)))&1);
}

int next_piece(struct peer_state *peer, int previous_piece, int *offset) {
    // Mark previous piece as finished downloading,
    // -1 means no previous piece (e.g., peer was just unchoked)
    if(previous_piece!=-1)
	piece_status[previous_piece].status=PIECE_FINISHED;

    draw_state();

    // Find the next piece to ask for
    for(int i=0;i<(file_length/piece_length+1);i++) {
	// Found a piece that is completely empty and that the peer actually has
	if (piece_status[i].status==PIECE_EMPTY && has_piece(peer,i)) {
	    if(debug)
		fprintf(stderr,"Next piece %d / %d\n",i,file_length/piece_length);
	    piece_status[i].status=PIECE_PENDING;
	    *offset = 0;
	    return i;
	}
	// Found a piece partially downloaded.
	// Request piece starting at the last byte we received.
	if (piece_status[i].status==PIECE_PENDING && has_piece(peer,i)) {
	    *offset = piece_status[i].offset;
	    return i;
	}
    }
    return -1;
}

/* This needs to be fixed to work properly for multi-file torrents.
   specifically, it needs to create the proper directory structure, rather than just concatenate directory and file names.
*/
void write_block(char* data, int piece, int offset, int len, int acquire_lock) {
    FILE *outfile;

    int accumulated_file_length = 0;
    int block_start = piece*piece_length+offset;

    struct bencode_list* files = (struct bencode_list*)ben_dict_get_by_str(info,"files");
    // multi-file case
    if(files) {
	for(int i=0;i<files->n;i++) {
	    struct bencode* file = files->values[i];
	    struct bencode_list* path = (struct bencode_list*)ben_dict_get_by_str(file,"path");
	    //			printf("Filename %s/%s\n",((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,((struct bencode_str*)path->values[0])->s);
	    // accumulate a total length so we know how many pieces there are
	    int file_length=((struct bencode_int*)ben_dict_get_by_str(file,"length"))->ll;

	    printf("start %d len %d accum %d filelen %d\n",block_start,len,accumulated_file_length,file_length);
	    fflush(stdout);
	    // at least part of the block belongs in this file
	    if((block_start >= accumulated_file_length) && (block_start < accumulated_file_length+file_length)) {
		char filename[255];

		mkdir(((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,0777);
		chmod(((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,07777);

		sprintf(filename,"%s/",((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s);
		for(int j=0;j<path->n;j++) {
		    if(j<(path->n-1)) {
			sprintf(filename+strlen(filename),"%s/",((struct bencode_str*)path->values[j])->s);
			mkdir(filename,0777);
			chmod(filename,07777);
		    }
		    else
			sprintf(filename+strlen(filename),"%s",((struct bencode_str*)path->values[j])->s);
		}

		int outfile = open(filename,O_RDWR|O_CREAT,0777);
		if(outfile == -1) {
		    fprintf(stderr,"filename: %s\n",filename);
		    perror("Couldn't open file for writing");
		    exit(1);
		}

		int offset_into_file = block_start - accumulated_file_length;
		int remaining_file_length = file_length - offset_into_file;
		lseek(outfile,offset_into_file,SEEK_SET);

		if(remaining_file_length > len) {
		    write(outfile,data,len);
		    close(outfile);
		}
		else {
		    if(debug) {
			fprintf(stderr,"Uh-oh, write crossing file boundaries... watch out!\n");
			fprintf(stderr,"Len %d offset %d filelen %d remaining file len %d\n",len,offset_into_file,file_length,remaining_file_length);
			fflush(stdout);
		    }

		    write(outfile,data,remaining_file_length);
		    close(outfile);
		    write_block(data+remaining_file_length,piece,offset+remaining_file_length,len-remaining_file_length,0);
		}

	    }
	    accumulated_file_length+=file_length;
	}
    }
    // single-file case
    else {

	struct bencode_str* name = (struct bencode_str*)ben_dict_get_by_str(info,"name");
	if(name) {

	    int outfile = open(name->s,O_RDWR|O_CREAT,0777);


	    file_length = ((struct bencode_int*)ben_dict_get_by_str(info,"length"))->ll;

	    // write the data to the right spot in the file
	    lseek(outfile,piece*piece_length+offset,SEEK_SET);
	    write(outfile, data,len);
	    close(outfile);


	}
	else {
	    printf("No name?\n");
	    exit(1);
	}
    }
}

void buffer_message(struct peer_state *peer, const void *msg, int len) {
    memcpy(peer->outgoing+peer->outgoing_count, msg, len);
    peer->outgoing_count += len;
    FD_SET(peer->socket, &writeset);
}

// Wait for peer to send us another full message, then return the length of the new message.
int receive_message(struct peer_state* peer) {
    if (peer->count<4 || ntohl(((int*)peer->incoming)[0])+4 > peer->count) {
	int newbytes=recv(peer->socket,peer->incoming+peer->count,BUFSIZE-peer->count,0);
	if(newbytes == 0) {
	    // Connection closed by peer
	    return 0;
	}
	else if(newbytes < 0) {
	    perror("Problem when receiving more bytes, closing socket.");
	    shutdown_peer(peer);
	    return 0;
	}
	peer->count+=newbytes;
    }
    // Wait for whole message to arrive
    if (peer->count<4 || ntohl(((int*)peer->incoming)[0])+4 > peer->count) {
	return 0;
    }
    return ntohl(((int*)peer->incoming)[0]);
}

// Drop the most recent message from the buffer.
void drop_message(struct peer_state* peer) {
    int msglen = ntohl(((int*)peer->incoming)[0]); // size of length prefix is not part of the length
    if(peer->count<msglen+4) {
	fprintf(stderr,"Trying to drop %d bytes, we have %d!\n",msglen+4,peer->count);
	peer->connected=0;
	exit(1);
    }
    peer->count -= msglen+4; // size of length prefix is not part of the length
    if(peer->count) {
	memmove(peer->incoming,peer->incoming+msglen+4,peer->count);
    }
}

void request_block(struct peer_state* peer, int piece, int offset) {

    /* requests have the following format */
    struct {
	int len;
	char id;
	int index;
	int begin;
	int length;
    } __attribute__((packed)) request;

    request.len=htonl(13);
    request.id=6;
    request.index=htonl(piece);
    request.begin=htonl(offset);
    request.length=htonl(1<<14);

    // the last block is likely to be of non-standard size
    int maxlen = file_length - (piece*piece_length+offset);
    if(maxlen < (1<<14))
	request.length = htonl(maxlen);

    // no point in sending anything if we got choked. We'll restart on unchoke.
    // WARNING: not handling the case where we get choked in the middle of a piece! Does this happen?
    if(!peer->choked) {
	buffer_message(peer, &request, sizeof(request));
	/* send(peer->socket,&request,sizeof(request),0); */
    }
    else
	fprintf(stderr,"Not sending, choked!\n");
}


void send_interested(struct peer_state* peer) {
    struct {
	int len;
	char id;
    } __attribute__((packed)) msg;
    msg.len = htonl(1);
    msg.id = 2;

    /* send(peer->socket,&msg,sizeof(msg),0); */
    buffer_message(peer, &msg, sizeof(msg));
}

void shutdown_peer(struct peer_state *peer) {
    peer->connected = 0;
    FD_CLR(peer->socket, &readset);
    FD_CLR(peer->socket, &writeset);
    close(peer->socket);
}

void connect_to_peer(struct peer_addr *peeraddr) {
    fprintf(stderr,"Connecting...\n");

    if(peer_connected(peeraddr->addr, peeraddr->port)) {
	fprintf(stderr,"Already connected\n");
	return;
    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = peeraddr->addr;
    addr.sin_port = peeraddr->port;

    /* after 60 seconds of nothing, we probably should poke the peer to see if we can wake them up */
    struct timeval tv;
    tv.tv_sec = 60;
    tv.tv_usec = 0;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,  sizeof tv)) {
	perror("setsockopt");
	return;
    }

    int res = connect(s, (struct sockaddr*)&addr, sizeof(addr));
    if(res == -1) {
	fprintf(stderr,"Couldn't connect to %hu \n", peeraddr->port);
	fflush(stderr);
	perror("Error while connecting.");
	return;
    }

    /* send the handshake message */
    char protocol[] = "BitTorrent protocol";
    unsigned char pstrlen = strlen(protocol); // not sure if this should be with or without terminator
    unsigned char buf[pstrlen+49];
    buf[0]=pstrlen;
    memcpy(buf+1,protocol,pstrlen);
    memcpy(buf+1+pstrlen+8,digest,20);
    memcpy(buf+1+pstrlen+8+20,peer_id,20);

    /* register the new peer in our list of peers */
    struct peer_state* peer = calloc(1,sizeof(struct peer_state));
    peer->socket = s;
    peer->ip=peeraddr->addr;
    peer->port=peeraddr->port;
    peer->next = peers;
    peer->connected=0;
    peer->rcv_handshake = 0;
    peer->choked=1;
    peer->incoming=malloc(BUFSIZE);
    peer->outgoing=malloc(BUFSIZE);
    peer->outgoing_count = peer->count = 0;
    peer->bitfield = calloc(1,file_length/piece_length/8+1); //start with an empty bitfield
    peers=peer;

    //TODO: buffer handshake message before send!
    buffer_message(peer, buf, sizeof(buf));
}

void receive_handshake(struct peer_state *peer) {
    /* receive the handshake message. This is different from all other messages, making things ugly. */
    if (peer->count < 4 || peer->count < peer->incoming[0]+49) {
	int newbytes = recv(peer->socket,peer->incoming+peer->count,BUFSIZE-peer->count,0);
	if(newbytes==0) {
	    perror("receiving handshake");
	    shutdown_peer(peer);
	    return;
	}
	peer->count+=newbytes;
    }
    // Wait for handshake to continue another iteration of the select() loop
    if (peer->count < 4 || peer->count < peer->incoming[0]+49) {
	return;
    }

    if(memcmp(peer->incoming+peer->incoming[0]+1+8+20,"-OBCS342-",strlen("-OBCS342-"))==0) {
	fprintf(stderr,"Caught a CS342 peer, exiting.\n");
	shutdown_peer(peer);
	return;
    }

    // forget handshake packet
    if(debug)
	printf("handshake message is %d bytes\n",peer->count);

    peer->count -= peer->incoming[0]+49;
    if(peer->count)
	memmove(peer->incoming,peer->incoming+peer->incoming[0]+49,peer->count);

    peer->connected = 1;
    peer->rcv_handshake = 1;
}

void handle_message(struct peer_state *peer) {
    int newbytes=0;

    int msglen = receive_message(peer);

    fflush(stdout);
    if(msglen == 0) {
	peer->connected = 0;
	close(peer->socket);
	piece_status[peer->requested_piece].status=PIECE_EMPTY;
	draw_state();
	shutdown_peer(peer);
	return;
    };

    switch(peer->incoming[4]) {
	// CHOKE
    case 0: {
	if(debug)
	    fprintf(stderr,"Choke\n");
	peer->choked = 1;
	piece_status[peer->requested_piece].status=PIECE_EMPTY;
	peer->requested_piece = -1;
	break;
    }
	// UNCHOKE
    case 1: {
	if(debug)
	    fprintf(stderr,"Unchoke\n");
	peer->choked = 0;

	// grab a new piece - WARNING: this assumes that you don't get choked mid-piece!
	int offset = 0;
	peer->requested_piece = next_piece(peer, -1, &offset);
	request_block(peer,peer->requested_piece, offset);
	break;
    }
	// HAVE -- update the bitfield for this peer
    case 4: {
	int piece_index = ntohl(*((int*)&peer->incoming[5]));
	int bitfield_byte = piece_index/8;
	int bitfield_bit = 7-(piece_index%8);
	if(debug)
	    fprintf(stderr,"Have %d\n",piece_index);
	// OR the appropriate mask byte with a byte with the appropriate single bit set
	peer->bitfield[bitfield_byte]|=1<<bitfield_bit;

	send_interested(peer);
	break;
    }
	// BITFIELD -- set the bitfield for this peer
    case 5:
	peer->choked = 0; // let's assume a bitfield means we're allowed to go...
	if(debug)
	    printf("Bitfield of length %d\n",msglen-1);
	int fieldlen = msglen - 1;
	if(fieldlen != (file_length/piece_length/8+1)) {
	    fprintf(stderr,"Incorrect bitfield size, expected %d\n",file_length/piece_length/8+1);
	    shutdown_peer(peer);
	}
	memcpy(peer->bitfield,peer->incoming+5,fieldlen);

	send_interested(peer);
	break;
	// PIECE
    case 7: {
	int piece = ntohl(*((int*)&peer->incoming[5]));
	int offset = ntohl(*((int*)&peer->incoming[9]));
	int datalen = msglen - 9;

	fprintf(stderr,"Writing piece %d, offset %d, ending at %d\n",piece,offset,piece*piece_length+offset+datalen);
	write_block(peer->incoming+13,piece,offset,datalen,1);

	draw_state();
	offset+=datalen;
	if(offset==piece_length || (piece*piece_length+offset == file_length) ) {

	    if(debug)
		fprintf(stderr,"Reached end of piece %d at offset %d\n",piece,offset);

	    peer->requested_piece=next_piece(peer, piece, &offset);

	    if(peer->requested_piece==-1) {
		fprintf(stderr,"No more pieces to download!\n");
		shutdown_peer(peer);
	    }
	}

	request_block(peer,peer->requested_piece,offset);
	break;
    }

    case 20:
	PRINT("Extended type received");
	break;
    }
    drop_message(peer);
}

/* handle_announcement reads an announcement document to find some peers to download from.
   start a new tread for each peer.
*/
void handle_announcement(char *ptr, size_t size) {
    struct bencode* anno = ben_decode(ptr,size);

    printf("Torrent has %lld seeds and %lld downloading peers. \n",
	   ((struct bencode_int*)ben_dict_get_by_str(anno,"complete"))->ll,
	   ((struct bencode_int*)ben_dict_get_by_str(anno,"incomplete"))->ll);

    struct bencode_list *peers = (struct bencode_list*)ben_dict_get_by_str(anno,"peers");

    // handle the binary case
    if(peers->type == BENCODE_STR) {
	printf("Got binary list of peers\n");

	// the "string" in peers is really a list of peer_addr structs, so we'll just cast it as such
	struct peer_addr *peerlist = (struct peer_addr*)((struct bencode_str*)peers)->s;
	for(int i=0;i<((struct bencode_str*)peers)->len/6;i++) {
	    struct in_addr a;
	    a.s_addr = peerlist[i].addr;
	    printf("Found peer %s:%d\n",inet_ntoa(a),ntohs(peerlist[i].port));

	    connect_to_peer(&peerlist[i]);
	}
    }
    // handle the bencoded case
    else {
	for(int i=0;i<peers->n;i++) {
	    printf("Got bencoded list of peers\n");
	    struct bencode *peer = peers->values[i];
	    char *address = ((struct bencode_str*)ben_dict_get_by_str(peer,"ip"))->s;
	    unsigned short port = ((struct bencode_int*)ben_dict_get_by_str(peer,"port"))->ll;
	    printf("Found peer %s:%d\n",address,port);

	    struct peer_addr *peeraddr = malloc(sizeof(struct peer_addr));
	    peeraddr->addr=inet_addr(address);
	    peeraddr->port=htons(port);

	    connect_to_peer(peeraddr);
	}
    }
}

/* contact the tracker to get announcement, call handle_announcement on the result */
void start_peers() {
    /* now download the announcement document using libcurl.
       because of the way curl does things, it's easiest to just throw the entire document into a file first,
       and then just read the file. the alternative would be to buffer up all the data in memory using a
       custom callback function. Let's stick with the KISS principle.
    */
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if(curl) {
	curl_easy_setopt(curl, CURLOPT_URL, announce_url);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1);

	char *tmp_file = "/tmp/anno-llovett.tmp";
	FILE *anno = fopen(tmp_file,"w+");
	if(!anno) {
	    perror("couldn't create temporary file\n");
	}

	int attempts=0;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, anno);
	while((res = curl_easy_perform(curl)) !=CURLE_OK &&
	      attempts < 5) {
	    fprintf(stderr, "curl_easy_perform() failed: %s\n",
		    curl_easy_strerror(res));
	    attempts++;
	}
	fclose(anno);

	if (attempts<5) {
	    struct stat anno_stat;
	    if(stat(tmp_file,&anno_stat)) {
		perror("couldn't stat temporary file");
		exit(1);
	    }
	    // the announcement document is in /tmp/anno.tmp.
	    // so map that into memory, then call handle_announcement on the returned pointer
	    handle_announcement(mmap(0,anno_stat.st_size,PROT_READ,MAP_SHARED,open(tmp_file,O_RDONLY),0),anno_stat.st_size);
	}
	curl_easy_cleanup(curl);
    }
}

int main(int argc, char** argv) {
    if(argc<2) {
	fprintf(stderr,"Usage: ./hw4 <torrent file>\n");
	exit(1);
    }

    setvbuf(stdout,screenbuf,_IOFBF,10000);

    // create a global peer_id for this session. Every peer_id should include -OBCS342-.
    for(int i=strlen(peer_id);i<20;i++)
	peer_id[i]='0'+random()%('Z'-'0'); // random numbers/letters between 0 and Z

    // make sure the torrent file exists
    struct stat file_stat;
    if(stat(argv[1],&file_stat)) {
	perror("Error opening file.");
	exit(1);
    }

    // map .torrent file into memory, and parse contents
    int fd = open(argv[1],O_RDONLY);
    char *buf = mmap(0,file_stat.st_size,PROT_READ,MAP_SHARED,fd,0);
    if(buf==(void*)-1) {
	perror("couldn't mmap file");
	exit(1);
    }
    size_t off = 0;
    int error = 0;
    torrent = (struct bencode_dict*)ben_decode2(buf,file_stat.st_size,&off,&error);
    if(!torrent) {
	printf("Got error %d, perhaps a malformed torrent file?\n",error);
	exit(1);
    }

    // pull out the .info part, which has stuff about the file we're downloading
    info = (struct bencode*)ben_dict_get_by_str((struct bencode*)torrent,"info");

    struct bencode_list* files = (struct bencode_list*)ben_dict_get_by_str(info,"files");
    // multi-file case
    if(files) {
	for(int i=0;i<files->n;i++) {
	    struct bencode* file = files->values[i];
	    struct bencode_list* path = (struct bencode_list*)ben_dict_get_by_str(file,"path");
	    printf("Filename %s/%s\n",((struct bencode_str*)ben_dict_get_by_str(info,"name"))->s,((struct bencode_str*)path->values[0])->s);

	    // accumulate a total length so we know how many pieces there are
	    file_length+=((struct bencode_int*)ben_dict_get_by_str(file,"length"))->ll;
	}
    }
    // single-file case
    else {
	struct bencode_str* name = (struct bencode_str*)ben_dict_get_by_str(info,"name");
	if(name) {
	    file_length = ((struct bencode_int*)ben_dict_get_by_str(info,"length"))->ll;
	}
    }
    fflush(stdout);
    piece_length = ((struct bencode_int*)ben_dict_get_by_str(info,"piece length"))->ll;

    //start with an empty bitfield
    piece_status = calloc(1,sizeof(struct piece_status_t)*(int)(file_length/piece_length+1));

    /* compute the message digest and info_hash from the "info" field in the torrent */
    size_t len;
    char info_hash[100];
    char* encoded = ben_encode(&len,(struct bencode*)info);
    SHA1(encoded,len,digest); // digest is a global that holds the raw 20 bytes

    // info_hash is a stringified version of the digest, for use in the announce URL
    memset(info_hash,0,100);
    for(int i=0;i<20;i++)
	sprintf(info_hash+3*i,"%%%02x",digest[i]);


    // compile a suitable announce URL for our document
    sprintf(announce_url,"%s?info_hash=%s&peer_id=%s&port=6881&left=%d",
	    ((struct bencode_str*)ben_dict_get_by_str((struct bencode*)torrent,"announce"))->s,
	    info_hash,peer_id,file_length);
    printf("Announce URL: %s\n",announce_url);
    fflush(stdout);

    // Find peers from the tracker
    start_peers();
    // Add all peers to global read set
    for (struct peer_state *peer=peers; peer; peer=peer->next) {
	FD_SET(peer->socket, &readset);
    }

    // Select loop
    while (1) {
	fd_set rtemp, wtemp;
	memcpy(&rtemp, &readset, sizeof(fd_set));
	memcpy(&wtemp, &writeset, sizeof(fd_set));

	int selected = select(FD_SETSIZE, &rtemp, &wtemp, NULL, 0);
	if (selected < 0) {
	    perror("select");
	    /* exit(1); */
	}

	struct peer_state *peer = peers;
	while (peer) {
	    if (FD_ISSET(peer->socket, &wtemp) &&
		(peer->connected || !peer->rcv_handshake)) {
		int msgsize = peer->outgoing_count;
		int sent_bytes = send(peer->socket, peer->outgoing, msgsize, MSG_NOSIGNAL);
		if (sent_bytes < 0) {
		    // Problem with send... let's ignore it!!! :)
		    continue;
		}
		memmove(peer->outgoing, peer->outgoing+sent_bytes, peer->outgoing_count-sent_bytes);
		peer->outgoing_count -= sent_bytes;
		if (peer->outgoing_count == 0) {
		    FD_CLR(peer->socket, &writeset);
		}
	    }
	    if (FD_ISSET(peer->socket, &rtemp)) {
		if (!peer->connected) {
		    receive_handshake(peer);
		} else if (receive_message(peer)) {
		    handle_message(peer);
		}
	    }
	    peer = peer->next;
	}

	// wait for a signal that all the downloading is done
	if (missing_blocks()>0) {
	    if(active_peers()==0 && missing_blocks()>0) {
		printf("Ran out of active peers, reconnecting.\n");
		start_peers();
	    }
	} else {
	    puts("Download complete");
	    break;
	}
    }

    return 0;
}


#include "tcp.h"

int
main(int argc, char** argv) {
    int len;
    Data* dat = (Data*)calloc(1, sizeof(Data));
    Header* hdr = (Header*)calloc(1, sizeof(Header));

    tcp_socket();
    len = recv_tcp_packet(hdr, dat);
    
    printf("Received %d bytes! Dumping header & content:\n", len);
    dump_header(hdr);
    dump_buffer(dat->content, dat->len);
    
    free(dat->content);
    free(dat);
    free(hdr);

    return 1;
}


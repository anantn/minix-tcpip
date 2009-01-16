#include "tcp.h"

#define MSG "Hello World!"
int
main(int argc, char** argv)
{
    Data* dat = (Data*)calloc(1, sizeof(Data));
    Header* hdr = (Header*)calloc(1, sizeof(Header));
    
    hdr->dst = inet_addr("192.168.0.1");
    hdr->sport = 9090;
    hdr->dport = 9090;
    hdr->seqno = 1;
    /* Only set data offset */
    hdr->flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT;
    
    dat->len = 12;
    dat->content = (uchar*)MSG;
    
    tcp_socket();
/*    printf("Sent %d bytes!\n", send_tcp_packet(hdr, dat)); */
    printf("Sent %d bytes!\n", send_tcp_packet(hdr->dst, hdr->sport, hdr->dport, hdr->seqno, 0, 0, 10, MSG, strlen(MSG)));
    
    free(dat);
    free(hdr);
    return 1;
}

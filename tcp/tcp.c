#include "tcp.h"

/* Low level send and receive functions */
int
send_tcp_packet(Header* hdr, Data* dat)
{   
    uchar* tmp;
    int len, csum;
    
    if (!hdr->dst) {
        return 0;
    }
    
    hdr->src = my_ipaddr;
    hdr->prot = htons(IP_PROTO_TCP);
    hdr->tlen = htons(HEADER_SIZE + dat->len);
    swap_header(hdr, 0);
    
    tmp = (uchar*)calloc(sizeof(Header) + dat->len, sizeof(uchar));
    memcpy(tmp, (uchar*)hdr, sizeof(Header));
    memcpy(tmp + sizeof(Header), dat->content, dat->len);
    csum = raw_checksum(tmp, sizeof(Header) + dat->len);
    memcpy(tmp + CHECK_OFF, (uchar*)&csum, sizeof(u16_t));

    len = ip_send(hdr->dst, IP_PROTO_TCP, 2,
                    (void*)(tmp + HEADER_OFF), HEADER_SIZE + dat->len);
    free(tmp);
    
    return len;
}

int
recv_tcp_packet(Header* hdr, Data* dat)
{
    char* data;
    uchar* tmp;
    int len, doff;
    ushort prot, idp;
    ipaddr_t src, dst;
    
    len = ip_receive(&src, &dst, &prot, &idp, &data);

    /* Extract header - pseudo and real, still in network order */
    hdr->src = src;
    hdr->dst = dst;
    hdr->prot = htons(prot);
    hdr->tlen = htons(len);
    memcpy(((uchar*)hdr) + HEADER_OFF, data, HEADER_SIZE);
    
    /* Check the checksum! */
    tmp = (uchar*)calloc(HEADER_OFF + len, sizeof(uchar));
    memcpy(tmp, (uchar*)hdr, HEADER_OFF);
    memcpy(tmp + HEADER_OFF, (uchar*)data, len);
    if (raw_checksum(tmp, HEADER_OFF + len) != 0) {
        free(tmp);
        dprint("Incorrect Checksum! Discarding packet...\n");
        return -1;
    }
    free(tmp);
    
    /* Back to host order */
    hdr->prot = prot;
    hdr->tlen = len;
    swap_header(hdr, 1);
    
    /* We ignore options, just interested in data offset
       (which is described in number of WORD_SIZE bytes).
     */
    doff = (hdr->flags >> DATA_SHIFT) * WORD_SIZE;
    dat->len = len - doff;    
    dat->content = (uchar*)calloc(dat->len, sizeof(uchar));
    memcpy(dat->content, (uchar*)data + doff, dat->len);
    
    return len;
}

/* Init TCP: Single connection for now */
int
tcp_socket()
{
    TCPCtl* ctl;
    if (ip_init()) {
        Head = (TCPMux*)calloc(1, sizeof(TCPMux));
        ctl = (TCPCtl*)calloc(1, sizeof(TCPCtl));
        
        Head->socket = 1;
        Head->sport = 9090;
        Head->this = ctl;
        Head->next = NULL;
        
        return 1;
    }
    
    return 0;
}

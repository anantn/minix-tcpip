#include "tcp.h"

/* This function simply calculates the checksum over a raw buffer */
u16_t
raw_checksum(uchar* buf, int buflen)
{
    u32_t sum=0;
    u16_t u16_end=0;
    u16_t *u16_buf = (u16_t *)buf;
    int u16_buflen = buflen >> 1;

    if (buflen % 2) {
        *((u8_t *)&u16_end) = ((u8_t *)buf)[buflen-1];
        sum += u16_end;
    }

    while (u16_buflen--) {
        sum += u16_buf[u16_buflen];
        if (sum >= 0x00010000)
            sum -= 0x0000ffff;
    }

    return (u16_t)~sum;
}

/* Swaps endianess on a Header structure */
void
swap_header(Header* hdr, int ntoh)
{
    if (ntoh) {
        hdr->sport = ntohs(hdr->sport);
        hdr->dport = ntohs(hdr->dport);
        hdr->seqno = ntohl(hdr->seqno);
        hdr->ackno = ntohl(hdr->ackno);
        hdr->flags = ntohs(hdr->flags);
        hdr->window = ntohs(hdr->window);
        hdr->chksum = ntohs(hdr->chksum);
        hdr->urgent = ntohs(hdr->urgent);
    } else {
        hdr->sport = htons(hdr->sport);
        hdr->dport = htons(hdr->dport);
        hdr->seqno = htonl(hdr->seqno);
        hdr->ackno = htonl(hdr->ackno);
        hdr->flags = htons(hdr->flags);
        hdr->window = htons(hdr->window);
        hdr->chksum = htons(hdr->chksum);
        hdr->urgent = htons(hdr->urgent);
    }
}

/* shows compact TCP packet */
void show_packet (Header * hdr, uchar * buf, int len )
{
	int i ;
	ddprint ("HDR{");
    ddprint("[%s:%d --> %s:%d] ", inet_ntoa(hdr->src), hdr->sport, inet_ntoa(hdr->dst), hdr->dport) ;
	ddprint ("[S%d:A%d] [W:%d] [", hdr->seqno, hdr->ackno,hdr->window );
	if (hdr->flags & SYN) ddprint( "S");
	if (hdr->flags & ACK ) ddprint( "A");
	if (hdr->flags & FIN ) ddprint( "F");
	if (hdr->flags & RST) ddprint( "R");
	ddprint("]}  DATA{%d[", len);
	for (i = 0 ; i < len ; ++i ) ddprint ( "%c", buf[i] );
	ddprint ("]} %s\n", state_names[Head->this->state]);

}

/* Prints a TCP Header */
void
dump_header(Header* hdr)
{
    ddprint("Header Dump\n");
    ddprint("-----------\n");
    ddprint("SRC: %s, %0X\n", inet_ntoa(hdr->src), hdr->src);
    ddprint("DST: %s, %0X \n", inet_ntoa(hdr->dst), hdr->dst);
    ddprint("PRT: %d, %0X \n", hdr->prot, hdr->prot);
    ddprint("LEN: %d, %0X \n", hdr->tlen, hdr->tlen);
    ddprint("SPT: %d, %0X \n", hdr->sport, hdr->sport);
    ddprint("DPT: %d, %0X \n", hdr->dport, hdr->dport);
    ddprint("SEQ: %d, %0X \n", hdr->seqno, hdr->seqno);
    ddprint("ACK: %d, %0X \n", hdr->ackno, hdr->ackno);
    ddprint("FLG: %X \n", hdr->flags);
    ddprint("WIN: %X %d\n", hdr->window, hdr->window);
    ddprint("CHK: %X \n", hdr->chksum);
    ddprint("URG: %X \n", hdr->urgent);
    ddprint("-----------\n");
}

/* Prints a buffer and its ASCII values */
void
dump_buffer(uchar* buf, int len)
{
    int i;
    ddprint("Buffer Dump, length: %d\n", len);
    ddprint("-----------------------\n");
    for (i = 0; i < (len / 4); i++) {
        ddprint("%0X %0X %0X %0X", buf[i*4] & 0xff, buf[i*4+1] & 0xff, buf[i*4+2] & 0xff, buf[i*4+3] & 0xff);
        ddprint("\n");
        ddprint("%c %c %c %c", buf[i*4], buf[i*4+1], buf[i*4+2], buf[i*4+3]);
        ddprint("\n\n");
    }
}

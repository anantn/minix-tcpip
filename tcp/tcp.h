#ifndef _TCP_H
#define _TCP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "ip.h"

#define VERBOSE 1
#ifdef DEBUG
 #ifdef VERBOSE  
  #define dprint printf
 #else
  #define dprint noprint
 #endif /* endif VERBOSE */
 #define ddprint printf
#else /* else DEBUG */
 #define dprint noprint
 #define ddprint noprint
#endif

#ifndef IP_PROTO_TCP
#define IP_PROTO_TCP 6
#endif

#ifdef LINUX
#define inet_addr inet_aton
#endif

#define DATA_SIZE   8148
#define PACKET_SIZE 8192
#define HEADER_SIZE 20
#define CHECK_OFF   28
#define HEADER_OFF  12
#define DATA_SHIFT  12
#define WORD_SIZE   4
#define RETRANSMISSION_TIMER 1

#define MIN(a,b) (a) < (b) ? (a) : (b) 
typedef unsigned char uchar;
typedef unsigned short ushort;

enum {
    URG = 0x20,
    ACK = 0x10,
    PSH = 0x08,
    RST = 0x04,
    SYN = 0x02,
    FIN = 0x01,
    
    TCP_LISTEN = 0,
    TCP_CONNECT = 1,
    
    Closed = 0,
    Listen,
    Syn_Sent,
    Syn_Recv,
    Established,
    Fin_Wait1,
    Fin_Wait2,
    Close_Wait,
    Closing,
    Last_Ack,
    Time_Wait
};
    

typedef struct _Header {
    /* First, the pseudoHeader */
    u32_t src;
    u32_t dst;
    u16_t prot;
    u16_t tlen;
    /* Real header starts here */
    u16_t sport;
    u16_t dport;
    u32_t seqno;
    u32_t ackno;
    /* flags include offset, reserved and control bits */
    u16_t flags;
    u16_t window;
    u16_t chksum;
    u16_t urgent;
} Header;
    
typedef struct _Data {
    u32_t len;
    uchar* content;
} Data;

typedef struct _TCPCtl {
    uchar state;
    uchar type;
    
    u32_t local_seqno;
    u32_t remote_ackno;
    u32_t remote_seqno;
    u32_t remote_window ;
    
    Data* in_buffer;
	Data* out_buffer;
    int remaining;
} TCPCtl;

typedef struct _TCPMux {
    int socket;
    u16_t sport;
    u16_t dport;
    ipaddr_t dst;
    
    TCPCtl* this;
    TCPCtl* next;
} TCPMux;

/* global state (single connection for now) */
extern TCPMux* Head;
extern char state_names[][12];

/* public interface */
int tcp_socket(void);
int tcp_connect(ipaddr_t dst, int port);
int tcp_listen(int port, ipaddr_t *src);
int tcp_read(char* buf, int maxlen);
int tcp_write(char* buf, int len);
int tcp_close(void);

/* Low level interface wrapper */
int
send_tcp_packet(ipaddr_t dst, u16_t src_port, u16_t dst_port,
	u32_t seq_nb, u32_t ack_nb, u8_t flags, u16_t win_sz,
	const char* data, int data_sz) ;
int
recv_tcp_packet(ipaddr_t* src, u16_t* src_port, u16_t* dst_port,
	u32_t* seq_nb, u32_t* ack_nb, u8_t* flags, u16_t* win_sz,
	char* data, int* data_sz);

int _recv_tcp_packet(Header* hdr, Data* dat);
int _send_tcp_packet(Header* hdr, Data* dat);

/* util functions */
u16_t raw_checksum(uchar* dat, int len);
void tcp_checksum(Header* hdr, Data* dat, int con);
void dump_header(Header* hdr);
void dump_buffer(uchar* dat, int len);
void swap_header(Header* hdr, int ntoh);
void show_packet(Header * hdr, uchar * buf, int len );

/* more support functions*/
int can_read(int state);
int can_write(int state);
int handle_packets(void);
int send_ack(void) ;
int setup_packet(Header *hdr );
int wait_for_ack(u32_t local_seqno);
int write_packet(char * buf, int len, int flags );

/* state handling functions*/
int handle_Closed_state(Header *hdr, Data *dat);
int handle_Listen_state(Header *hdr, Data *dat);
int handle_Syn_Sent_state(Header *hdr, Data *dat);
int handle_Syn_Recv_state(Header *hdr, Data *dat);
int handle_Established_state(Header *hdr, Data *dat);
/* signal handling function */
void alarm_signal_handler(int sig) ;

/* for debugging support */
int noprint (char *fmt, ...) ;

#endif /* _TCP_H */

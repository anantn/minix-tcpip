#ifndef _TCP_H
#define _TCP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "ip.h"

#ifdef DEBUG 
  #define dprint printf
#else
  #define dprint noprint
#endif

#ifndef IP_PROTO_TCP
#define IP_PROTO_TCP 6
#endif

#ifdef LINUX
#define inet_addr inet_aton
#endif

#define MIN(a,b) (a) < (b) ? (a) : (b) 
typedef unsigned char uchar;
typedef unsigned short ushort;

extern int CURRUPT_THIS_PACKET ;

/* Some constants we will use later */
#define DATA_SIZE   8148
#define PACKET_SIZE 8192
#define START_PORT	8090
#define MAX_CONN	256
#define HEADER_SIZE 20
#define CHECK_OFF   28
#define HEADER_OFF  12
#define DATA_SHIFT  12
#define WORD_SIZE   4
#define RETRANSMISSION_TIMER 1

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

/* A TCPv4 header */
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
    /* The flags field includes offset, reserved and control bits */
    u16_t flags;
    u16_t window;
    u16_t chksum;
    u16_t urgent;
} Header;

typedef struct _Data {
    u32_t len;
    uchar* content;
} Data;

/* One structure per connection */
typedef struct _TCPCtl {
	int socket;
	u16_t sport;
	u16_t dport;
	ipaddr_t dst;

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

/* Public interface */
int tcp_socket(void);
int tcp_connect(ipaddr_t dst, int port);
int tcp_connect_socket(int socket, ipaddr_t dst, int port);
int tcp_listen(int port, ipaddr_t *src);
int tcp_listen_socket(int socket, int port, ipaddr_t *src);
int tcp_read(char* buf, int maxlen);
int tcp_read_socket(int socket, char* buf, int maxlen);
int tcp_write(char* buf, int len);
int tcp_write_socket(int socket, char* buf, int len);
int tcp_close(void);
int tcp_close_socket(int socket);

#endif

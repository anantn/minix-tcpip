/*
 * IP definitions (see RFC-791)
 */
#ifndef __IP_H__
#define __IP_H__

#ifdef __minix
#include "inet.h"
#else
typedef u_int8_t u8_t;
typedef u_int16_t u16_t;
typedef u_int32_t u32_t;
typedef unsigned long ipaddr_t;
unsigned short inet_checksum(void *, int);
#endif

/* generic IP address */
#define	IP_ANYADDR	(ipaddr_t)0	/* IP any address */
#define	IP_BCASTADDR	(ipaddr_t)-1 	/* IP broadcast address */

#define	IP_PROTO_UDP	17		/* UDP protocol number */

/*
 * Structure of an internet header (without options)
 */
typedef struct {
    unsigned char	ip_vhl;		/* version and header length */
#define	IP_VERSION	4		/* current version number */
    unsigned char	ip_tos;		/* type of service */
    short		ip_len;		/* total length */
    unsigned short	ip_id;		/* identification */
    short		ip_off;		/* fragment offset field */
#define	IP_DF		0x4000		/* don't fragment flag */
#define	IP_MF		0x2000		/* more fragments flag */
#define	IP_OFFMASK	0x1FFF		/* fragment offset mask */
    unsigned char	ip_ttl;		/* time to live */
#define	IP_FRAGTTL	60		/* time to live for frags */
    unsigned char	ip_p;		/* protocol */
    unsigned short	ip_sum;		/* checksum */
    ipaddr_t		ip_src;		/* source address */
    ipaddr_t		ip_dst;		/* destination address */
} iphdr_t;

int ip_init(void);
int ip_send(ipaddr_t dst, unsigned short proto, 
	    unsigned short id, void *data, int len);
int ip_receive(ipaddr_t *srcp, ipaddr_t *dstp, unsigned short *protop, 
	       unsigned short *idp, char **data);
int extra_info(void);

extern ipaddr_t my_ipaddr;
#endif /* __IP_H__ */

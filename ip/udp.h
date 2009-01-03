/*
 * udp.h
 */
#ifndef __UDPIP_H__
#define __UDPIP_H__

#define	UDP_ANYPORT	0	/* receive UDP packets from any port */

typedef unsigned short port_t;	/* UDP port type */

/*
 ^ The structure of an UDP header
 */
typedef struct {
    port_t		uh_sport;	/* source port */
    port_t		uh_dport;	/* destination port */
    short		uh_len;		/* udp packet lenght */
    unsigned short	uh_sum;		/* udp checksum */
} udphdr_t;

/*
 * Useful routines ...
 */
port_t udp_port(void);
int    udp_send(port_t sport, ipaddr_t daddr, 
		port_t dport, void *data, int len);
int    udp_receive(port_t *dportp, ipaddr_t *saddrp, 
		   port_t *sportp, void *data, int size);

#endif /* __UDPIP_H__ */


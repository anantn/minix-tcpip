/*
 * Ethernet Address Resolution Protocol (see RFC 826)
 */
#ifndef __ARP_H__
#define __ARP_H__

#define	ARP_NRETRIES	3			/* # of ARP & RARP retries */
#define	ARP_NENTRIES	20			/* # of ARP table entries */

/*
 * ARP packets are variable in size; the arphdr_t type defines the
 * 10Mb Ethernet variant.  Field names used correspond to RFC 826.
 */
typedef struct {
    unsigned short	arp_hrd;		/* format of hardware addr */
#define	ARPHRD_ETHER	1       		/* ethernet hardware addr */
    unsigned short	arp_pro;		/* format of proto. addr */
    unsigned char	arp_hln;		/* length of hardware addr */
    unsigned char	arp_pln;		/* length of protocol addr */
    unsigned short 	arp_op;
#define	ARPOP_REQUEST	1			/* request to resolve addr */
#define	ARPOP_REPLY	2			/* response to prev request */
#define	REVARP_REQUEST	3			/* reverse ARP request */
#define	REVARP_REPLY	4			/* reverse ARP reply */
    unsigned char	arp_sha[ETH_ADDRSIZE];	/* sender hardware addr */
    unsigned char	arp_spa[4];		/* sender protocol addr */
    unsigned char	arp_tha[ETH_ADDRSIZE];	/* target hardware addr */
    unsigned char	arp_tpa[4];		/* target protocol addr */
} arphdr_t;

ipaddr_t	rarp(void);
unsigned char	*arp(ipaddr_t addr);
void		arp_request(ethaddr_t *src, char *data, int size);

#endif /* __ARP_H__ */




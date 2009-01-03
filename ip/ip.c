/*
 * ip.c
 *
 * Send and receive IP datagrams,
 * does full fragmentation and reassembly.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "ether.h"
#include "inet.h"
#include "udp.h"
#include "ip.h"
#include "arp.h"

#define	ETH_IPMTU	(ETH_MAXTU - sizeof(iphdr_t))
#define	NETMASK(a, m)	((a) & (m))

/*
 * The reassembly scheme used is based on periodic checking rather
 * than timeouts. The main reason for this is that the only interval
 * timer that's available under Unix is used by higher layers.
 *
 * When a fragment is encountered for the first time, a ipfraghdr is
 * allocated containing the tuple <src, dst, proto, id>, timers, head
 * and tail of the fragment data list, and a fragment bit table. The
 * individual fragments are placed in a list and the fragment bit
 * table is used to determine whether the whole datagram has been
 * received. Once the datagram size is known, i.e. after we've received
 * a message with no IP_MF set, this is also stored in the header.
 *
 * The individual fragments are stored in a linked list pointed to
 * by ifh_head. New fragments are appended to this list to ensure
 * that in the case of overlapping fragments the most recent one
 * takes precedence.
 * 
 * The fragment list is pruned every time ip_receive is called, and
 * every time ip_receive receive's an ethernet packet.
 */
typedef struct ipfrag {
    struct ipfrag	*if_next;	/* next fragment data */
    short		if_len;		/* fragment data length */
    unsigned short	if_off;		/* current offset */
    void		*if_data;	/* fragment data */
} ipfrag_t;

typedef struct ipfraghdr {
    struct ipfraghdr	*ifh_next;	/* next fragment header */
    time_t		ifh_expire;	/* when this entry expires */
    ipaddr_t		ifh_src;	/* source address */
    ipaddr_t		ifh_dst;	/* destination address */
    unsigned char 	ifh_proto;	/* protocol */
    unsigned short	ifh_id;		/* identifier */
    ipfrag_t		*ifh_head;	/* head of fragment list */
    ipfrag_t		*ifh_tail;	/* tail of fragment list */
    unsigned short	ifh_len;	/* length of whole datagram */
    unsigned char	ifh_table[8192];/* fragment bit table */
    /* XXX this table should be 1024 bytes, each fragment should
     * contain a multiple of eight bytes. */
} ipfraghdr_t;

static ipfraghdr_t *fragments = NULL;

/*
 * These are used by the RARP/ARP module
 */
ipaddr_t my_ipgateway	= IP_ANYADDR;
ipaddr_t my_ipaddr	= IP_ANYADDR;
ipaddr_t my_ipnetmask	= IP_ANYADDR;

static int drop_send= 0;
static int drop_send_packet;
static int drop_recv= 0;
static int drop_recv_packet;
static int extra_info_flag= 0;

int extra_info(void)
{
    return extra_info_flag;
}

/*
 * Initialize the IP layer
 */
int
ip_init(void)
{
    char *env;

#ifdef DEBUG
    fprintf(stderr,"YES! LEENDERT's LIBUDP USED\n");
#endif

    if (getenv("EXTRA_INFO"))
    {
        extra_info_flag = 1;
        fprintf(stderr, "extra info hack enabled\n");
    }
    if (getenv("DROP_SEND"))
    {
    	drop_send= 1;
    	drop_send_packet= 0;
    	fprintf(stderr, "drop send hack enabled\n");
    }
    if (getenv("DROP_RECV"))
    {
    	drop_recv= 1;
    	drop_recv_packet= 0;
    	fprintf(stderr, "drop recv hack enabled\n");
    }

    if (!(my_ipaddr = rarp()))
	return 0;

    env = getenv("IPNETMASK");
    if (env == NULL) {
	/* best effort guess */
	if ((ntohl(my_ipaddr) & 0x80000000) == 0) /* class A */
	    my_ipnetmask = htonl(0xFF000000);
	else if ((ntohl(my_ipaddr) & 0xC0000000) == 0x80000000) /* class B */
	    my_ipnetmask = htonl(0xFFFF0000);
	else if ((ntohl(my_ipaddr) & 0xE0000000) == 0xC0000000) /* class C */
	    my_ipnetmask = htonl(0xFFFFFF00);
    } else
    	my_ipnetmask = inet_addr(env);
    if (extra_info()) fprintf(stderr, "netmask = %s\n", inet_ntoa(my_ipnetmask));

    env = getenv("IPGATEWAY");
    if (env != NULL)
    	my_ipgateway = inet_addr(env);
    if (extra_info()) fprintf(stderr, "gateway = %s\n", inet_ntoa(my_ipgateway));
    return 1;
}

/*
 * Send IP packet, possibly fragmenting it
 */
int
ip_send(ipaddr_t dst, unsigned short proto, 
	unsigned short id, void *data, int len)
{
    register iphdr_t *ip;
    unsigned char *edst;
    char msg[ETH_MAXTU];
    ipaddr_t addr;
    long off;
    int iplen, elen;

    /*
     * Initialize the IP layer
     */
    if (!my_ipaddr && !ip_init())
    {
#ifdef DEBUG
        printf("ip: init failed?\n");
#endif
	return -1;
    }

    if (drop_send)
    {
    	drop_send_packet= 1-drop_send_packet;
    	if (drop_send_packet == 0)
    		return len;
    }

    /*
     * When the destination is not on the same network,
     * we have to use the gateway. For this to work we
     * need a netmask.
     */
    addr = dst;
    if (NETMASK(addr, my_ipnetmask) != NETMASK(my_ipaddr, my_ipnetmask))
	if ((addr = my_ipgateway) == 0)
        {
#ifdef DEBUG
            printf("ip: no gateway\n");
#endif
	    return -1;
        }

    /*
     * Find IP-address's ethernet address
     */
    if ((edst = arp(addr)) == NULL)
    {
#ifdef DEBUG
        printf("ip: eth-adres not found\n");
#endif
	return -1;
    }

    /*
     * Fragment data into multiple IP packets
     */
    for (off = 0, iplen = 0; off < len; off += iplen) {
	ip = (iphdr_t *) msg;
	iplen = len - off > ETH_IPMTU ? ETH_IPMTU : len - off;
	ip->ip_vhl = (IP_VERSION << 4) | (sizeof(*ip) >> 2);
	ip->ip_tos = 0;
	ip->ip_len = htons(iplen + sizeof(iphdr_t));
	ip->ip_id = htons(id);
	ip->ip_off = (short)off >> 3;
	if ((off + iplen) < len)
	    ip->ip_off |= IP_MF;
	ip->ip_off = htons(ip->ip_off);
	ip->ip_ttl = IP_FRAGTTL;
	ip->ip_p = (unsigned char) proto;
	ip->ip_sum = 0;
	ip->ip_src = my_ipaddr;
	ip->ip_dst = dst;
	ip->ip_sum = inet_checksum(ip, sizeof(*ip));
	memcpy(msg + sizeof(iphdr_t), (char *)data + off, iplen);
	if ((elen = iplen + sizeof(iphdr_t)) < ETH_MINTU)
	    elen = ETH_MINTU;
	if (eth_send((void*)edst, ETHTYPE_IP, msg, elen) < 0)
        {
#ifdef DEBUG
            printf("ip:eth_send failed\n");
#endif
	    return -1;
        }
    }
    return len;
}

/*
 * Bit set and test routines for the fragment bitmap
 */
static void
bset(unsigned char *bt, int s, int l)
{
    register int i;

    for (i = s >> 3; i < (s + l) >> 3; i++)
	bt[i] = (unsigned char)~0;
    for (i = (s + l) & ~7; i < s + l; i++)
	bt[i >> 3] |= (1 << (i & 7));
}

static int
btest(unsigned char *bt, int s, int l)
{
    register int i;

    for (i = s >> 3; i < (s + l) >> 3; i++)
	if (bt[i] != (unsigned char) ~0)
	    return 0;
    for (i = (s + l) & ~7; i < s + l; i++)
	if ((bt[i >> 3] & (1 << (i & 7))) == 0)
	    return 0;
    return 1;
}

/*
 * Add a fragment to the fragment list (possibly allocating a
 * new fragment header), and return a completed datagram, if any.
 */
static ipfraghdr_t *
ip_fragment(iphdr_t *ip)
{
    register ipfraghdr_t *ifhp;
    register ipfrag_t *ifp;

    /*
     * Find fragment header, or allocate a new one
     */
    for (ifhp = fragments; ifhp; ifhp = ifhp->ifh_next)
	if (ifhp->ifh_proto == ip->ip_p && ifhp->ifh_src == ip->ip_src &&
	  ifhp->ifh_dst == ip->ip_dst && ifhp->ifh_id == ip->ip_id)
	    break;
    if (ifhp == NULL) {
	if ((ifhp = malloc(sizeof(ipfraghdr_t))) == NULL) {
	   printf("%s, %d: malloc failed\n", __FILE__, __LINE__);
	   return NULL;
	}
	ifhp->ifh_src = ip->ip_src;
	ifhp->ifh_dst = ip->ip_dst;
	ifhp->ifh_proto = ip->ip_p;
	ifhp->ifh_id = ip->ip_id;
	ifhp->ifh_len = 0;
	ifhp->ifh_head = NULL;
	ifhp->ifh_tail = NULL;
	memset(ifhp->ifh_table, 0, sizeof(ifhp->ifh_table));
	ifhp->ifh_expire = 0;
	ifhp->ifh_next = fragments;
	fragments = ifhp;
    }

    /*
     * Add fragment to the end of the list, so that in the case of two
     * or more overlapping fragments the most recent arrived one will
     * be copied into the data packet.
     * Set the corresponding bits in the fragment receive table.
     */
    if ((ifp = malloc(sizeof(ipfrag_t))) == NULL) {
	printf("%s, %d: malloc failed\n", __FILE__, __LINE__);
	return NULL;
    }
    ifp->if_len = ip->ip_len - sizeof(iphdr_t);
    ifp->if_off = (ip->ip_off & IP_OFFMASK) << 3;
    if ((ifp->if_data = malloc(ifp->if_len)) == NULL) {
	printf("%s, %d: malloc failed\n", __FILE__, __LINE__);
	free(ifp);
	return NULL;
    }
    memcpy(ifp->if_data, ip+1, ifp->if_len);

    bset(ifhp->ifh_table, ifp->if_off >> 3, (ifp->if_len+7) >> 3);
    ifhp->ifh_expire = time(0) + ip->ip_ttl;

    ifp->if_next = NULL;
    if (ifhp->ifh_tail != NULL) {
	ifhp->ifh_tail->if_next = ifp;
	ifhp->ifh_tail = ifp;
    } else
	ifhp->ifh_tail = ifhp->ifh_head = ifp;

    /*
     * Once we got the last packet we know the actual size of
     * the whole datagram. From that point on we check whether
     * we received all the fragments.
     */
    if ((ip->ip_off & IP_MF) == 0)
	ifhp->ifh_len = ((ip->ip_off & IP_OFFMASK) << 3) +
	    ip->ip_len - sizeof(iphdr_t);
    if (ifhp->ifh_len > 0 &&
      btest(ifhp->ifh_table, 0, ((unsigned)ifhp->ifh_len + 7) >> 3))
	return ifhp;
    else
	return NULL;
}

/*
 * Reassemble the individual datagrams into one big packet. 
 * All the validity checks have already been performed.
 */
static void *
ip_reassemble(ipfraghdr_t *ifhp, int *len)
{
    register ipfrag_t *ifp;
    register char *data;

    if ((data = malloc(align(ifhp->ifh_len))) == NULL) {
	printf("%s, %d: malloc failed\n", __FILE__, __LINE__);
	return NULL;
    }
    for (ifp = ifhp->ifh_head; ifp; ifp = ifp->if_next)
	memcpy(data + ifp->if_off, ifp->if_data, ifp->if_len);
    *len = ifhp->ifh_len;
    return data;
}

/*
 * Free IP fragment information
 */
static void
ip_freefrag(ipfraghdr_t *ifhp)
{
    register ipfrag_t *ifp, *next;
    register ipfraghdr_t *p;

    if (fragments != ifhp) {
	for (p = fragments; p->ifh_next; p = p->ifh_next) {
	    if (p->ifh_next == ifhp) {
		p->ifh_next = p->ifh_next->ifh_next;
		break;
	    }
	}
    } else
	fragments = ifhp->ifh_next;

    for (ifp = ifhp->ifh_head, next = NULL; ifp; ifp = next) {
	free(ifp->if_data);
	next = ifp->if_next;
	free(ifp);
    }
    free(ifhp);
}

/*
 * Cleanup timed out fragments
 */
static void
ip_cleanfrags(void)
{
    register ipfraghdr_t *ifhp, *next;
    register time_t now;

    now = time(0);
    for (ifhp = fragments, next = NULL; ifhp; ifhp = next) {
	next = ifhp->ifh_next;
	if (ifhp->ifh_expire < now)
	    ip_freefrag(ifhp);
    }
}

/*
 * Receive any IP packet, possibly reassembling it
 */
int
ip_receive(ipaddr_t *srcp, ipaddr_t *dstp, unsigned short *protop, 
	   unsigned short *idp, char **data)
{
    ethaddr_t src, dst;
    char msg[ETH_MAXTU];
    unsigned short proto;
    iphdr_t *ip;
    int len, iplen;
    ipfraghdr_t *ipfraghdr;

    /*
     * Initialize the IP layer
     */
    if (!my_ipaddr && !ip_init())
	return -1;

    for (;;) {
	/*
	 * Clean up timed out fragments
	 */
	ip_cleanfrags();

    	if ((len = eth_receive(&src, &dst, &proto, msg, sizeof(msg))) < 0)
	    return -1;

	/*
	 * Possible ARP request for our IP address
	 */
	if (proto == ETHTYPE_ARP || proto == ETHTYPE_RARP) {
	    arp_request(&src, msg, len);
	    continue;
	}
	if (proto != ETHTYPE_IP)
	    continue;

	/*
	 * An IP packet, is it for me?
	 * (also accept IP broadcasts)
	 */
	ip = (iphdr_t *) msg;
	if (len < sizeof(iphdr_t))
	    continue;
	if (inet_checksum(ip, sizeof(*ip)))
	    continue;
	if (ip->ip_dst != my_ipaddr && ip->ip_dst != IP_BCASTADDR)
	    continue;

	/*
	 * The IP packet is for me. The packet is either a whole
	 * message or a fragment. Handle the easy case first.
	 */
	ip->ip_id = ntohs(ip->ip_id);
	ip->ip_off = ntohs(ip->ip_off);
	ip->ip_len = ntohs(ip->ip_len);
	if ((ip->ip_off & IP_MF) == 0 && (ip->ip_off & IP_OFFMASK) == 0) {
	    /* no fragmentation */
	    iplen = ip->ip_len - sizeof(iphdr_t);
	    if ((*data = malloc(align(iplen))) == NULL) {
		printf("%s, %d: malloc failed\n", __FILE__, __LINE__);
		return -1;
	    }
	    *srcp = ip->ip_src;
	    *dstp = ip->ip_dst;
	    *idp = ip->ip_id;
	    *protop = ip->ip_p;
	    memcpy(*data, msg + sizeof(iphdr_t), iplen);

	    if (drop_recv)
	    {
		drop_recv_packet= 1-drop_recv_packet;
		if (drop_recv_packet == 0) {
			free(*data);
			continue;
		}
	    }

	    return iplen;
	} else {
	    /* it is a fragment */
	    if ((ipfraghdr = ip_fragment(ip)) != NULL) {
		if ((*data = ip_reassemble(ipfraghdr, &iplen)) == NULL)
		    continue;
		*srcp = ipfraghdr->ifh_src;
		*dstp = ipfraghdr->ifh_dst;
		*idp = ipfraghdr->ifh_id;
		*protop = ipfraghdr->ifh_proto;
		ip_freefrag(ipfraghdr);

		if (drop_recv)
		{
		    drop_recv_packet= 1-drop_recv_packet;
		    if (drop_recv_packet == 0) {
			    free(*data);
			    continue;
		    }
		}

		return iplen;
	    }
	}
    }
}

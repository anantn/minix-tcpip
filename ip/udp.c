/*
 * udp.c
 *
 * Send and receive UDP datagrams.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ether.h"
#include "inet.h"
#include "udp.h"
#include "ip.h"
#include "arp.h"

#ifndef min
#define	min(a,b)	((a) < (b) ? (a) : (b))
#endif

/*
 * Pseudo header to compute UDP checksum.
 * The values in this structure are in network byte order!
 */
typedef struct {
    ipaddr_t		ph_src;
    ipaddr_t		ph_dst;
    unsigned char	ph_zero;
    unsigned char	ph_p;
    unsigned short	ph_len;
} pseudohdr_t;

/*
 * Return an unused (free) UDP port number
 */
port_t
udp_port(void)
{
    static long port = 2048;

    if (port > 65536)
	port = 2048;
    return port++;
}

/*
 * Send an UDP packet from port sport to host ``daddr''
 * and port ``dport''. The arguments are in host byte order.
 */
int
udp_send(port_t sport, ipaddr_t daddr, port_t dport, void *data, int len)
{
    static unsigned short ipid = 1;
    udphdr_t *up;
    char *msg;
    int rv, udplen;
    pseudohdr_t ph;
    extern ipaddr_t my_ipaddr;

    if (!my_ipaddr && !ip_init())
	return -1;

    if (extra_info())
    {
        printf("udp_send(%d, %s, %d, %p, %d)\n", sport, inet_ntoa(daddr), dport, data, len);
    }

    udplen = len + sizeof(udphdr_t);
    if ((msg = malloc(align(udplen))) == NULL) {
	printf("%s, %d: malloc failed\n", __LINE__, __FILE__);
	return -1;
    }
    up = (udphdr_t *) msg;
    up->uh_sport = htons(sport);
    up->uh_dport = htons(dport);
    up->uh_len = htons(udplen);
    up->uh_sum = 0;
    memcpy(msg + sizeof(udphdr_t), data, len);
    /* compute UDP checksum */
    ph.ph_src = my_ipaddr;
    ph.ph_dst = daddr;
    ph.ph_zero = 0;
    ph.ph_p = IP_PROTO_UDP;
    ph.ph_len = up->uh_len;
    up->uh_sum = ~inet_checksum(&ph, sizeof(ph));
    if (udplen & 1)
	msg[udplen] = '\0';
    up->uh_sum = inet_checksum(up, align(udplen));
    if (up->uh_sum == 0)
	up->uh_sum = 0xFFFF;
    rv = ip_send(daddr, IP_PROTO_UDP, ipid++, msg, udplen);
    free(msg);
    /* return rv; */
    return len;
}

/*
 * Receive an UDP packet from port ``dport'' and return the
 * source address and port in saddr, respectively sportp.
 * When dport contains 0, any UDP will match and the destination
 * UDP port of the message will be stored in dportp.
 */
int
udp_receive(dportp, saddrp, sportp, data, size)
    ipaddr_t *saddrp;
    port_t *dportp, *sportp;
    void *data;
    int size;
{
    unsigned short proto, id;
    unsigned short oldsum;
    ipaddr_t src, dst;
    udphdr_t *up;
    char *msg;
    int udplen, len, userlen;
    pseudohdr_t ph;
    extern ipaddr_t my_ipaddr;

    if (!my_ipaddr && !ip_init())
	return -1;
    while ((len = ip_receive(&src, &dst, &proto, &id, &msg)) >= 0) {
	if (proto != IP_PROTO_UDP)
	    continue;
	if (len < sizeof(udphdr_t))
	    continue;
	up = (udphdr_t *) msg;
	udplen = ntohs(up->uh_len);
	if (udplen > len)
	    continue;
	if (*dportp != UDP_ANYPORT && *dportp != ntohs(up->uh_dport))
	    continue;
	/* if there's an UDP checksum, check it */
	if ((oldsum = up->uh_sum) != 0) {
	    if (oldsum == 0xFFFF)
		oldsum = 0;
	    /* compute checksum of pseudo header */
	    ph.ph_src = src;
	    ph.ph_dst = dst;
	    ph.ph_zero = 0;
	    ph.ph_p = (unsigned char)proto;
	    ph.ph_len = up->uh_len;
	    up->uh_sum = ~inet_checksum(&ph, sizeof(ph));
	    if (udplen & 1)
		msg[udplen] = '\0';
	    if (inet_checksum(up, align(udplen)) != oldsum)
		continue;
	}
	/* pass datagram on to the user */
	userlen = min(size, udplen - sizeof(udphdr_t));
	*dportp = ntohs(up->uh_dport);
	*sportp = ntohs(up->uh_sport);
	*saddrp = src;
	memcpy(data, msg + sizeof(udphdr_t), userlen);
	free(msg);
        if (extra_info())
        {
            printf("udp_receive(&%d, &%s, &%d, %p, %d) = %d\n", *dportp, inet_ntoa(*saddrp), *sportp, data, size, userlen);
        }
	return userlen;
    }
    if (extra_info())
    {
        printf("udp_receive() = -1\n");
    }
    return -1;
}

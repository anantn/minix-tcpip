/*
 * arp.c
 *
 * Ethernet (Reverse) Address Resolution Protocol
 * (see RFC 903, and 826)
 *
 * Author:
 *	Leendert van Doorn
 */
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>

#include "ether.h"
#include "inet.h"
#include "ip.h"
#include "arp.h"

/*
 * Internet to hardware address resolution table
 */
typedef struct {
    ipaddr_t		at_ipaddr;		/* internet address */
    unsigned char	at_eaddr[ETH_ADDRSIZE];	/* ethernet address */
    unsigned long	at_reftime;		/* time when referenced */
} arptab_t;

extern ipaddr_t my_ipaddr;		/* my IP address */
extern ethaddr_t my_ethaddr;		/* my ethernet address */

static ethaddr_t bcastaddr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static arptab_t arptab[ARP_NENTRIES];
static jmp_buf env;

static void
timeout(void)
{
    (void) signal(SIGALRM, (void (*)(int)) timeout);
    longjmp(env, 1);
}

/*
 * Broadcast a RARP request (i.e. who knows who I am)
 */
static void
rarp_whoami(void)
{
    char msg[ETH_MAXTU];
    arphdr_t *ap;

    ap = (arphdr_t *) msg;
    ap->arp_hrd = htons(ARPHRD_ETHER);
    ap->arp_pro = htons(ETHTYPE_IP);
    ap->arp_hln = ETH_ADDRSIZE;
    ap->arp_pln = sizeof(ipaddr_t);
    ap->arp_op = htons(REVARP_REQUEST);
    memcpy(ap->arp_sha, &my_ethaddr, ETH_ADDRSIZE);
    memcpy(ap->arp_tha, &my_ethaddr, ETH_ADDRSIZE);
    eth_send(&bcastaddr, ETHTYPE_RARP, ap, ETH_MINTU);
}

/*
 * Using the RARP request/reply exchange we try to obtain our
 * internet address (see RFC 903).
 * We need a timeout here, so we're very careful to save the
 * callers handler.
 */
ipaddr_t
rarp(void)
{
    ethaddr_t src, dst;
    unsigned short proto;
    char data[ETH_MAXTU];
    arphdr_t *ap;
    int retry = 0;
    int timo = 4;
    int len;
    void (*oldsig)(int);
    unsigned oldtimo;

    oldtimo = alarm(0);
    oldsig = signal(SIGALRM, (void (*)(int)) timeout);

    eth_init();
    if (setjmp(env) == 1) {
	timo <<= 1; /* exponential back-off */
	retry++;
    }
    if (retry == ARP_NRETRIES) {
	(void) signal(SIGALRM, oldsig);
	(void) alarm(oldtimo);
        if (extra_info()) fprintf(stderr, "rarp failed...\n");
	return IP_ANYADDR;
    }

    /*
     * Broadcast a who-am-i request,
     * and validate all the responses.
     */
    rarp_whoami();
    (void) alarm(timo);
    for (;;) {
	len = eth_receive(&src, &dst, &proto, data, sizeof(data));
	if (len < sizeof(arphdr_t))
	    continue;
	if (proto != ETHTYPE_RARP)
	    continue;
	ap = (arphdr_t *) data;
	if (ntohs(ap->arp_hrd) != ARPHRD_ETHER)
	    continue;
	if (ntohs(ap->arp_pro) != ETHTYPE_IP)
	    continue;
	if (ap->arp_hln != ETH_ADDRSIZE)
	    continue;
	if (ap->arp_pln != sizeof(ipaddr_t))
	    continue;
	if (ntohs(ap->arp_op) != REVARP_REPLY)
	    continue;
	if (memcmp(ap->arp_tha, &my_ethaddr, ETH_ADDRSIZE))
	    continue;
	(void) alarm(0);
	(void) signal(SIGALRM, oldsig);
	(void) alarm(oldtimo);
        if (extra_info()) fprintf(stderr, "my ip = %s\n",
                                  inet_ntoa(*(ipaddr_t*)ap->arp_tpa));
	return *(ipaddr_t *)ap->arp_tpa;
    }
}

/*
 * Broadcast an ARP packet (i.e. ask who has address "addr")
 */
static void
arp_whohas(ipaddr_t addr)
{
    register arphdr_t *ap;
    char msg[ETH_MAXTU];
	
    ap = (arphdr_t *) msg;
    ap->arp_hrd = htons(ARPHRD_ETHER);
    ap->arp_pro = htons(ETHTYPE_IP);
    ap->arp_hln = ETH_ADDRSIZE;
    ap->arp_pln = sizeof(ipaddr_t);
    ap->arp_op = htons(ARPOP_REQUEST);
    memcpy(ap->arp_sha, &my_ethaddr, ETH_ADDRSIZE);
    memcpy(ap->arp_spa, &my_ipaddr, sizeof(ipaddr_t));
    memcpy(ap->arp_tpa, &addr, sizeof(ipaddr_t));
    eth_send(&bcastaddr, ETHTYPE_ARP, ap, ETH_MINTU);
}

/*
 * Resolve IP address by issueing an ARP
 */
unsigned char *
arp(ipaddr_t addr)
{
    ethaddr_t src, dst;
    unsigned short proto;
    char data[ETH_MAXTU];
    arphdr_t *ap;
    int i, len;
    int retry = 0;
    int timo = 4;
    void (*oldsig)(int);
    unsigned oldtimo;
    arptab_t *atp;

    /*
     * Determine wether we already have the address in our
     * cache, if not ``atp'' points to the last free entry
     * or to the entry with the lowest reference time.
     */
    atp = NULL;
    for (i = 0; i < ARP_NENTRIES; i++) {
	if (arptab[i].at_ipaddr == addr) {
	    arptab[i].at_reftime = time((time_t *)0);
	    return arptab[i].at_eaddr;
	}
	if (arptab[i].at_reftime == 0 ||
	  atp == NULL || atp->at_reftime > arptab[i].at_reftime)
	    atp = &arptab[i];
    }

    oldtimo = alarm(0);
    oldsig = signal(SIGALRM, (void (*)(int)) timeout);

    if (setjmp(env) == 1) {
	timo <<= 1; /* exponential back-off */
	retry++;
    }
    if (retry == ARP_NRETRIES) {
	(void) signal(SIGALRM, oldsig);
	(void) alarm(oldtimo);
	return NULL;
    }

    /*
     * Broadcast a who-has request,
     * and validate all the responses.
     */
    arp_whohas(addr);
    (void) alarm(timo);
    for (;;) {
	len = eth_receive(&src, &dst, &proto, data, sizeof(data));
	if (len < sizeof(arphdr_t))
	    continue;
	if (proto != ETHTYPE_ARP && proto != ETHTYPE_RARP)
	    continue;
	ap = (arphdr_t *) data;
	if (ntohs(ap->arp_hrd) != ARPHRD_ETHER)
	    continue;
	if (ntohs(ap->arp_pro) != ETHTYPE_IP)
	    continue;
	if (ap->arp_hln != ETH_ADDRSIZE)
	    continue;
	if (ap->arp_pln != sizeof(ipaddr_t))
	    continue;
	if (ntohs(ap->arp_op) != ARPOP_REPLY)
	    continue;
	if (memcmp(ap->arp_tha, &my_ethaddr, ETH_ADDRSIZE) != 0)
	    continue;
	/* if (*(ipaddr_t *)ap->arp_tpa != my_ipaddr) */
	    /* continue; */
	(void) alarm(0);
	memcpy(atp->at_eaddr, ap->arp_sha, ETH_ADDRSIZE);
	atp->at_reftime = time((time_t *)0);
	atp->at_ipaddr = addr;
	(void) signal(SIGALRM, oldsig);
	(void) alarm(oldtimo);
	return atp->at_eaddr;
    }
}

/*
 * Handle ARP requests.
 * We only answer ARP requests for our own IP address.
 * The data buffer should hold at least ETH_MINTU bytes.
 */
void
arp_request(ethaddr_t *src, char *data, int size)
{
    arphdr_t *ap;

    if (size < sizeof(arphdr_t))
	return;
    ap = (arphdr_t *) data;
    if (ntohs(ap->arp_hrd) != ARPHRD_ETHER)
	return;
    if (ntohs(ap->arp_pro) != ETHTYPE_IP)
	return;
    if (ap->arp_hln != ETH_ADDRSIZE)
	return;
    if (ap->arp_pln != sizeof(ipaddr_t))
	return;
    if (ntohs(ap->arp_op) != ARPOP_REQUEST)
	return;
    if (*(ipaddr_t *)ap->arp_tpa != my_ipaddr)
	return;
    ap->arp_op = htons(ARPOP_REPLY);
    memcpy(ap->arp_sha, &my_ethaddr, ETH_ADDRSIZE);
    memcpy(ap->arp_spa, &my_ipaddr, sizeof(ipaddr_t));
    memcpy(ap->arp_tha, src, ETH_ADDRSIZE);
    eth_send(src, ETHTYPE_ARP, ap, ETH_MINTU);
}

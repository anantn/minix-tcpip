/* This program can be used to test normal three way handshake
 * and simultaneous three way handshake */

/* run this program as follows
 * $ ETH=2 ./tcp_connect */

#include "tcp.h"
#include <signal.h>

#define MSG "hello world!"
#define MSG2 "r u still there??"

void tp_signal_handler (int sig)
{
	fprintf(stderr,"############################ came in for %d signal, sigalarm is %d\n", sig, SIGALRM);
	fprintf(stderr,"\nso, quiting from here\n");
	exit (1);
}

int
main(int argc, char** argv)
{
	char info[DATA_SIZE] = MSG ;
	char buffer[DATA_SIZE] ;
   	ipaddr_t dst_ip = inet_addr("192.168.0.1");
/*    int sport = 9000;*/
    int dport = 8090;
	int len  ;
	int ret ;

    
	len = strlen (MSG);

	if ( signal(SIGALRM, tp_signal_handler) == SIG_ERR )
	{
		fprintf(stderr,"###### prob is sig setting in prog\n");
	}
	alarm(18) ;
    ret = tcp_socket();
	if (ret == -1 )
	{
		fprintf(stderr,"\nError: Prob in initializing IP lib\n");	
		exit(1);
	}
	fprintf(stderr,"\n====== socket created, trying to connect\n");
	tcp_connect (dst_ip, dport);
	fprintf(stderr,"\n====== sending data\n");
    fprintf(stderr,"\n====== Sent %d bytes!\n", tcp_write(info, len));
	len = tcp_read (info, DATA_SIZE);
  	fprintf(stderr,"\n====== Received answer of %d bytes, Dumping content:\n", len); 
	strncpy (buffer, info, len);
   	fprintf(stderr,"\n====== Data is %s \n", buffer);
   	fprintf(stderr," ====== Done\n");
	strcpy (info, MSG2) ;
	len = strlen (MSG2) ;
	fprintf(stderr,"\n====== sending next set of data\n");
/*	CURRUPT_PACKET_NOW = 1 ; */
    fprintf(stderr,"\n====== Sent %d bytes!\n", tcp_write(info, len));

	fprintf(stderr,"\n====== closing connection\n");
	tcp_close ();
   	fprintf(stderr,"\n====== Done closing\n");
	for (;;);
    return 1;
}

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
	printf ("############################ came in for %d signal, sigalarm is %d\n", sig, SIGALRM);
	printf ("\nso, quiting from here\n");
	exit (1);
}

int
main(int argc, char** argv)
{
	char info[DATA_SIZE] = MSG ;
	char buffer[DATA_SIZE] ;
   	ipaddr_t dst_ip = inet_addr("192.168.0.1");
/*    int sport = 9000;*/
    int dport = 6000;
	int len  ;
	int ret ;

    
	len = strlen (MSG);
    

	if ( signal(SIGALRM, tp_signal_handler) == SIG_ERR )
	{
		printf ("###### prob is sig setting in prog\n");
	}
	alarm(8) ;
    ret = tcp_socket();
	if (ret == 0 )
	{
		printf ("\nError: Prob in initializing IP lib\n");	
		exit(1);
	}
	printf ("\n====== socket created, trying to connect\n");
	tcp_connect (dst_ip, dport);
	printf ("\n====== sending data\n");
    printf("\n====== Sent %d bytes!\n", tcp_write(info, len));
	len = tcp_read (info, DATA_SIZE);
  	printf ("\n====== Received answer of %d bytes, Dumping content:\n", len); 
	strncpy (buffer, info, len);
   	printf ("\n====== Data is %s \n", buffer);
   	printf (" ====== Done\n");
	strcpy (info, MSG2) ;
	len = strlen (MSG2) ;
	printf ("\n====== sending next set of data\n");
	CURRUPT_PACKET_NOW = 1 ;
    printf("\n====== Sent %d bytes!\n", tcp_write(info, len));

	printf ("\n====== closing connection\n");
	tcp_close ();
   	printf ("\n====== Done closing\n");
	for (;;);
    return 1;
}

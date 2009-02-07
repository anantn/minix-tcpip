/* This program can be used to test normal three way handshake
 * and simultaneous three way handshake */

/* run this program as follows
 * $ ETH=1 ./tcp_connect_parallel */

#include "tcp.h"

#define MSG "bye bye world!"
int
main(int argc, char** argv)
{
	char info[DATA_SIZE] = MSG ;
   	ipaddr_t dst_ip = inet_addr("192.168.0.2");
/*    int sport = 9000;*/
    int dport = 6000;
	int len ;
	int ret, i ;

    
	
	len = strlen (MSG);
    
    ret = tcp_socket();
	if (ret == -1)
	{
		printf ("\n=== Error: Prob in initializing IP lib\n");	
		exit(1);
	}
	printf ("\n=== socket created, trying to connect\n");
	tcp_connect (dst_ip, dport);
	printf ("\n=== sending data\n");
    printf("\n=== Sent %d bytes!\n", tcp_write(info, len));
	len = tcp_read (info, DATA_SIZE);
   	printf ("\n=== Received answer of %d bytes, Dumping content:\n", len); 
   	/* dump_buffer ((uchar *)info, len); */
   	printf ("\n=== Data is \n=== ");
	for (i = 0 ; i < len ; ++i ) printf ("%c", info[i]);
   	printf (" === \n=== Done\n");
	printf ("\n=== closing connection\n");
	tcp_close ();
   	printf ("\n=== Done closing\n");
    return 1;
}

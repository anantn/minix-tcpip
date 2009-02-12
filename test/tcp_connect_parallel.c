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
    int dport = 8090;
	int len ;
	int ret, i ;

    
	
	len = strlen (MSG);
    
    ret = tcp_socket();
	if (ret == -1)
	{
		fprintf(stderr,"\n=== Error: Prob in initializing IP lib\n");	
		exit(1);
	}
	fprintf(stderr,"\n=== socket created, trying to connect\n");
	tcp_connect (dst_ip, dport);
	fprintf(stderr,"\n=== sending data\n");
    fprintf(stderr,"\n=== Sent %d bytes!\n", tcp_write(info, len));
	len = tcp_read (info, DATA_SIZE);
   	fprintf(stderr,"\n=== Received answer of %d bytes, Dumping content:\n", len); 
   	/* dump_buffer ((uchar *)info, len); */
   	fprintf(stderr,"\n=== Data is \n=== ");
	for (i = 0 ; i < len ; ++i ) fprintf(stderr,"%c", info[i]);
   	fprintf(stderr," === \n=== Done\n");
	fprintf(stderr,"\n=== closing connection\n");
	tcp_close ();
   	fprintf(stderr,"\n=== Done closing\n");
    return 1;
}

/* This program is designed to test read after close support */

/* run this program as follows
 * $ ETH=2 ./tcp_read_after_close_client */

#include "tcp.h"

#define MSG "hello world!"
#define MSG2 "r u still there??"


int
main(int argc, char** argv)
{
	char info[DATA_SIZE] = MSG ;
	char buffer[DATA_SIZE] ;
   	ipaddr_t dst_ip = inet_addr("192.168.0.1");
    int dport = 8090;
	int len  ;
	int ret ;

	len = strlen (MSG);
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
    strcpy (info, MSG2);
    len = strlen (MSG2);
    ret = tcp_write(info, len);
    fprintf (stderr, "sent %d bytes of data\n", len);
   	fprintf(stderr," ====== Done\n");
	fprintf(stderr,"\n====== closing connection\n");
	tcp_close ();
   	fprintf(stderr,"\n====== Done closing\n");
}

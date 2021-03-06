/* This program can be used to test the behaviour of library when SYN+ACK
 * packet is dropped in three way handshake
 **/

/* run this program as follows
 * $ ETH=2 ./tcp_syn_ack_drop_client */

#include "tcp.h"

#define MSG "hello world!"
#define MSG2 "r u still there??"

int
main(int argc, char** argv)
{
	char info[DATA_SIZE] = MSG ;
	char buffer[DATA_SIZE] ;
   	ipaddr_t dst_ip = inet_addr("192.168.0.1");
    int dport = 6000;
	int len  ;
	int ret ;

    
	len = strlen (MSG);
    
    ret = tcp_socket();
	if (ret == -1 )
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
	printf ("\n====== closing connection\n");
	tcp_close ();
   	printf ("\n====== Done closing\n");
    return 1;
}

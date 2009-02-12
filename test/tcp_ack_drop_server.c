/* This program can be used to test normal three way handshake */

/* run this program as follows
 * $ ETH=1 ./tcp_listen */

#include "tcp.h"
#define ANSWER "Thanx for reply..!!!"
int
main(int argc, char** argv) {

	int ret, len  ;
	char info [DATA_SIZE];
	char buffer[DATA_SIZE];
	ipaddr_t src_ip ;
	int port_no = 1234 ;
/*	DROP_FLOW_DIRECTION = TCP_LISTEN ; 
	DROP_PACKET_NO = 1 ;*/
	
    ret = tcp_socket();
	if (ret == -1 )
	{
		printf ("\nError: Prob in initializing IP lib\n");	
		exit(1);
	}
	ret = tcp_listen (port_no, &src_ip); 

	printf ("\n===== listen done, trying to read data\n");
	
	while (1)
	{
		len = tcp_read (info, DATA_SIZE);
	    printf("\n===== Received %d bytes! Dumping content:\n", len);
		if (len == -1 )
		{
			printf ("\n===== connection closed by other side ") ;
			break ;	
		}
		strncpy (buffer, info, len);
		printf ("\n====== Data is %s \n", buffer);
		printf("\n===== Done\n");
    
		printf("\n===== Sending same data back\n" );
		printf("\n===== Sent %d bytes!\n", tcp_write(info, len));
	}
	printf("\n===== closing connection\n");
	tcp_close ();
   	printf("\n===== Done closing\n");
    return 1;
}


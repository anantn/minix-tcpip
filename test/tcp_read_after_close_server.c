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
	int port_no = 8090 ;
    ret = tcp_socket();
	if (ret == -1 )
	{
		fprintf(stderr,"\nError: Prob in initializing IP lib\n");	
		exit(1);
	}
	ret = tcp_listen (port_no, &src_ip); 

	fprintf(stderr,"\n===== listen done, trying to read data\n");
    len = tcp_read (info, DATA_SIZE);

    
    fprintf(stderr,"\n===== Received %d bytes! Dumping content:\n", len);
	if (len == -1 )
	{
		printf ("\n===== connection closed by other side ") ;
		return 1 ;	
	}
	strncpy (buffer, info, len);
   	fprintf(stderr,"\n====== Data is %s \n", buffer);
   	fprintf(stderr,"\n===== Done\n");
    
	fprintf(stderr,"\n===== Sending reply [%s]\n", ANSWER );
	strcpy (info, ANSWER);
	len = strlen (ANSWER);
    fprintf(stderr,"\n===== Sent %d bytes!\n", tcp_write(info, len));
	fprintf(stderr,"\n===== closing connection\n");
	tcp_close ();

    fprintf(stderr,"reading after close\n");
    len = tcp_read(info, DATA_SIZE);
    if (len == -1 )
    {
        fprintf(stderr,"Other size has closed the connection\n");
        return 0 ;
    }
    fprintf(stderr,"Msg with len %d received\n", len);
    write (stderr, info, len);

   	fprintf(stderr,"\nDone\n");
    return 1;
}


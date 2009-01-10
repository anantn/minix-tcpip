/* This program can be used to test normal three way handshake */

/* run this program as follows
 * $ ETH=1 ./tcp_listen */

#include "tcp.h"
#define ANSWER "Thanx for reply..!!!"
int
main(int argc, char** argv) {

	int ret, len ;
	char info [DATA_SIZE];
	ipaddr_t src_ip ;
	int port_no = 3000 ;
//    Data* dat = (Data*)calloc(1, sizeof(Data));
//    Header* hdr = (Header*)calloc(1, sizeof(Header));
    ret = tcp_socket();
	if (ret == 0 )
	{
		printf ("\nError: Prob in initializing IP lib\n");	
		exit(1);
	}
	ret = tcp_listen (port_no, &src_ip); 

	printf ("listen done, trying to read data\n");
    len = tcp_read (info, DATA_SIZE);

    
    printf("Received %d bytes! Dumping content:\n", len);
//    dump_header(hdr);
    dump_buffer(info,len );
    
	printf ("Sending reply [%s]\n", ANSWER );
	strcpy (info, ANSWER);
	len = strlen (ANSWER);
    printf("Sent %d bytes!\n", tcp_write(info, len));
    return 1;
}


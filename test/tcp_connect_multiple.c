/* This program can be used to test multiple connetion support
*/

/* run this program as follows
 * $ ETH=2 ./tcp_connect_multiple */

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
    int socket_1, socket_2 ;
	int ret ;

	len = strlen (MSG);
    socket_1 = tcp_socket();
	if (socket_1 == -1 )
	{
		fprintf(stderr,"\nError: Prob in initializing IP lib\n");	
		exit(1);
	}
    socket_2 = tcp_socket();
	if (socket_2 == -1 )
	{
		fprintf(stderr,"\nError: Prob in initializing IP lib\n");	
		exit(1);
	}



	fprintf(stderr,"\n====== sockets created, trying to connect\n");
	ret = tcp_connect_socket (socket_1, 8090, dport);
    if (ret == -1 )
    {
        fprintf(stderr,"\nconnect failed for socket_1\n");
    }
   	ret = tcp_connect_socket (socket_2, 8091, dport);
    if (ret == -1 )
    {
        fprintf(stderr,"\nconnect failed for socket_2\n");
    }



	fprintf(stderr,"\n====== sending data\n");
    fprintf(stderr,"\n====== Sent %d bytes on socket_1\n", tcp_write_socket(socket_1, info, len));
    fprintf(stderr,"\n====== Sent %d bytes on socket_2\n", tcp_write_socket(socket_2, info, len));
	len = tcp_read_socket (socket_1, info, DATA_SIZE);
  	fprintf(stderr,"\n====== Received answer of %d bytes on socket_1\n", len); 
    write (stderr, info, len);
   	fprintf(stderr," ====== Done\n");
	len = tcp_read_socket (socket_2, info, DATA_SIZE);
  	fprintf(stderr,"\n====== Received answer of %d bytes on socket_2\n", len); 
    write (stderr, info, len);
   	fprintf(stderr," ====== Done\n");

	fprintf(stderr,"\n====== closing connections\n");
	tcp_close_socket (socket_1);
	tcp_close_socket (socket_2);
   	fprintf(stderr,"\n====== Done closing\n");
}

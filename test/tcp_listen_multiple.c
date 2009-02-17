/* This program can be used to test multiple socket support in library */

/* run this program as follows
 * $ ETH=1 ./tcp_listen_multiple */

#include "tcp.h"
#define ANSWER "Thanx for reply..!!!"
int
main(int argc, char** argv) {

	int ret, len  ;
	char info [DATA_SIZE];
	char buffer[DATA_SIZE];
	ipaddr_t src_ip ;
	int port_no = 8090 ;
    int socket_1, socket_2 ;
    socket_1 = tcp_socket();
	if (socket_1 == -1 )
	{
		printf ("\nError: Prob in initializing IP lib sock 1\n");	
		exit(1);
	}
    
    socket_2 = tcp_socket();
	if (socket_2 == -1 )
	{
		printf ("\nError: Prob in initializing IP lib sock 2\n");
		exit(1);
	}
	ret = tcp_listen_socket (socket_1, 8090, &src_ip);
    if (ret == -1)
    {
        printf ("ERROR: tcp_listen_socket, can't listen on given socket_1\n");
        exit(1);
    }
	ret = tcp_listen_socket (socket_2, 8091, &src_ip);
    if (ret == -1)
    {
        printf ("ERROR: tcp_listen_socket, can't listen on given socket_2\n");
        exit(1);
    }


	printf ("\n===== listen done, trying to read data\n");
    len = tcp_read_socket (socket_1, info, DATA_SIZE);
    printf("\n===== Received %d bytes! on socket 1\n", len);
   	printf ("\n====== Data is \n");
    write (stdout, info, len);
   	printf("\n===== Done\n");
     len = tcp_read_socket (socket_2, info, DATA_SIZE);
    printf("\n===== Received %d bytes! on socket 2\n", len);
   	printf ("\n====== Data is \n");
    write (stdout, info, len);
   	printf("\n===== Done\n");
    
	printf("\n===== Sending reply [%s]\n", ANSWER );
	strcpy (info, ANSWER);
	len = strlen (ANSWER);
    printf("\n===== Sent %d bytes on socket 1!\n", tcp_write_socket(socket_1, info, len));
    printf("\n===== Sent %d bytes on socket 2!\n", tcp_write_socket(socket_2, info, len));
	printf("\n===== closing connection\n");
	tcp_close_socket (socket_1);
	tcp_close_socket (socket_2);
   	printf(" ===== Done\n");
    return 1;
}


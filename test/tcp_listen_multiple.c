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

    fprintf(stderr,"\nHi there\n");
    socket_1 = tcp_socket();
	if (socket_1 == -1 )
	{
		fprintf(stderr,"\nError: Prob in initializing IP lib sock 1\n");	
/*		exit(1);*/
	}
    fprintf(stderr,"\nHi there2\n");
    
    socket_2 = tcp_socket();
	if (socket_2 == -1 )
	{
		fprintf(stderr,"\nError: Prob in initializing IP lib sock 2\n");
		exit(1);
	}
	ret = tcp_listen_socket (socket_1, 8090, &src_ip);
    if (ret == -1)
    {
        fprintf(stderr,"ERROR: tcp_listen_socket, can't listen on given socket_1\n");
        exit(1);
    }
	ret = tcp_listen_socket (socket_2, 8091, &src_ip);
    if (ret == -1)
    {
        fprintf(stderr,"ERROR: tcp_listen_socket, can't listen on given socket_2\n");
        exit(1);
    }


	fprintf(stderr,"\n===== listen done, trying to read data\n");
    len = tcp_read_socket (socket_1, info, DATA_SIZE);
    fprintf(stderr,"\n===== Received %d bytes! on socket 1\n", len);
   	fprintf(stderr,"\n====== Data is \n");
    write (stdout, info, len);
   	fprintf(stderr,"\n===== Done\n");
     len = tcp_read_socket (socket_2, info, DATA_SIZE);
    fprintf(stderr,"\n===== Received %d bytes! on socket 2\n", len);
   	printf ("\n====== Data is \n");
    write (stdout, info, len);
   	fprintf(stderr,"\n===== Done\n");
    
	fprintf(stderr,"\n===== Sending reply [%s]\n", ANSWER );
	strcpy (info, ANSWER);
	len = strlen (ANSWER);
    fprintf(stderr,"\n===== Sent %d bytes on socket 1!\n", tcp_write_socket(socket_1, info, len));
    fprintf(stderr,"\n===== Sent %d bytes on socket 2!\n", tcp_write_socket(socket_2, info, len));
	fprintf(stderr,"\n===== closing connection\n");
	tcp_close_socket (socket_1);
	tcp_close_socket (socket_2);
   	fprintf(stderr," ===== Done\n");
    return 1;
}


#include "tcp.h"

int
main(int argc, char** argv)
{
	char info[15] = "hello world!" ;
   	ipaddr_t dst_ip = inet_addr("192.168.0.1");
//    int sport = 9000;
    int dport = 3000;
	int len = 12 ;
	int ret ;

    
    
    ret = tcp_socket();
	if (ret == 0 )
	{
		printf ("\nError: Prob in initializing IP lib\n");	
		exit(1);
	}
	printf ("socket created, trying to connect\n");
	tcp_connect (dst_ip, dport);
	printf ("sending data\n");
    printf("Sent %d bytes!\n", tcp_write(info, len));
    
    return 1;
}

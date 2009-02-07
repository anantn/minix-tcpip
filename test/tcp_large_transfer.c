#include <tcp.h>

/* To run this program 
 ETH=2 ./tcp_large_transfer
 * */

int
main()
{
    char* buf;
	int ret ; 
    tcp_socket();
    if ( tcp_connect(inet_addr("192.168.0.1"), 1080) == -1 )
	{
		printf ("Other side is not ready\n");
	}
    
    buf = (char*)calloc(100001, sizeof(char));
    memset(buf, '1', 100001);
    ret = tcp_write(buf, 100001);
	printf ("sent %d bytes successfully\n", ret );
    memset(buf, '2', 100001);
/*	CURRUPT_PACKET_NOW = 1 ; */
    ret = tcp_write(buf, 100001);
	printf ("sent %d bytes successfully\n", ret );
    tcp_close();
    
    return 1;
}

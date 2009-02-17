#include <tcp.h>

/* To run this program 
 ETH=1 ./tcp_checksum_test_server
 * */

int
main()
{
    int i, len ;
    char* buf;
	
    tcp_socket() ; 
    tcp_listen(1080, (ipaddr_t*)my_ipaddr);
    
    buf = (char*)calloc(100, sizeof(char));
    
    i = 0;
    len = tcp_read(buf, 100);
	printf ("\nData received of size %d is \n", len );
	write (1, buf, len );
	printf ("\nechoing it back to server\n");
/*	CURRUPT_THIS_PACKET = 1 ; */
    i = tcp_write(buf, len);
	printf ("\nData sent of size %dis \n", i );
    tcp_close();
    return 0;
}

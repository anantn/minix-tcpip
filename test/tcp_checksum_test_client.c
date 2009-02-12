#include <tcp.h>

/* To run this program 
 ETH=2 ./tcp_checksum_test_client
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
    
    buf = (char*)calloc(100, sizeof(char));
	strcpy (buf, "foo ");
	printf ("sending data %s of len %d\n", buf, strlen(buf));
	CURRUPT_THIS_PACKET = 1 ;
    ret = tcp_write(buf, strlen(buf));
	printf ("sent %d bytes successfully\n", ret );
	printf ("waiting for response\n" );
    ret = tcp_read (buf, 100);
	printf ("got %d bytes from server\n", ret );
	printf ("the data is \n");
	write (1, buf, ret);
	printf ("\nDone.... now, closing the connection\n");
    tcp_close();
    
    return 0;
}

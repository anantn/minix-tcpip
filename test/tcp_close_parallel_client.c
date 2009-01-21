#include <tcp.h>

/* To run this program 
 ETH=2 ./tcp_close_parallel_client
 * */

int
main()
{
    char* buf;
    int i, j ;

	i = 0 ;
    tcp_socket();
    tcp_connect(inet_addr("192.168.0.1"), 1234);
    
    buf = (char*)calloc(100, sizeof(char));
    memset(buf, '1', 9);
    
	printf ("\nserver write req\n");
    tcp_write(buf, 10);
	printf ("\nserver write Done\n");
	printf ("\nserver read request\n");
    j = tcp_read(buf, 100);
	if ( j == -1 )
	{
		printf ("\nRead failed\n");
		return 0 ;
	}
    printf("%d bytes read!\ndata=\n", j);
	write (1, buf, j);
	printf ("\nserver read Done\n");
	printf ("\nserver close called\n");

    tcp_close();
	printf ("\nserver close done\n");
    
    return 1;
}

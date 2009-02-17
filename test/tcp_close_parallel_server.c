#include <tcp.h>
/* Objective : Both sides will call close simultaneously */

/* To run this program 
 ETH=1 ./tcp_close_parallel_server
 * */

#define MSG "foo"
int
main()
{
    int i, j;
    char* buf;
    
    tcp_socket();
    tcp_listen(1234, (ipaddr_t*)my_ipaddr);
    
    buf = (char*)calloc(1025, sizeof(char));
    
    i = 0;
    j = tcp_read(buf, 1024);
	if ( j == -1 )
	{
		printf ("\nRead failed\n");
		return 0 ;
	}
    printf("%d bytes read!\ndata=\n", j);
	write (1, buf, j);
	printf ("\nclient read Done\n");
	strcpy (buf, MSG); 
	i = strlen (MSG);
	printf ("\nclient write started\n");
	j = tcp_write (buf, i);
	printf ("client wrote %d bytes\n", j );
	printf ("client close called\n" );
    tcp_close();
	printf ("client close done\n" );
    return 1;
}

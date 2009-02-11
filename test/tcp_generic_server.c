#include <tcp.h>

/* To run this program 
 ETH=1 ./tcp_generic_server
 * */

int
main()
{
    int i, j, k;
    char* buf;
    int c1, c2 ;
	
    tcp_socket() ; 
    tcp_listen(1080, (ipaddr_t*)my_ipaddr);
    
    buf = (char*)calloc(200000, sizeof(char));
    
	c1 = 0 ;
	c2 = 0 ;
    i = 0;
    j = tcp_read(buf, 200000);
    while (j != EOF) {
/*        printf("%s", buf);*/
		for (k = 0 ; k < j ; ++k)
		{
			if (buf[k] == '1') ++c1 ;
			else ++c2 ;
		}
		
		fprintf (stderr, "Read %d bytes in this attempt 1 = %d,2 = %d\n", j, c1, c2 );
        i += j;
        j = tcp_read(buf, 1024);
    }
    fprintf(stderr, "\n");
    i += j;
    fprintf(stderr, "%d bytes read! in total 1 = %d, 2 = %d\n\n", i, c1, c2);
    
    tcp_close();
    return 1;
}

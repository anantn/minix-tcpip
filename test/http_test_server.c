#include <tcp.h>

int
main(void)
{
    char* dat;
	int len ;
    
    tcp_socket();
    tcp_connect(inet_addr("192.168.0.1"), 80);
    tcp_write("GET /foo.txt HTTP/1.1\r\n\r\n", 25);
    
    dat = (char*)calloc(32, sizeof(char));
    while ((len = tcp_read(dat, 32))!= -1) {
        printf ("\ndata len %d =\n", len ) ;
		write (1, dat, len );
		printf ("\ndone\n");
    }
    
    tcp_close();
    return 1;
}

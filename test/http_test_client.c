#include <tcp.h>

int
main(void)
{
    tcp_socket();
    tcp_listen(80, inet_addr("192.168.0.1"));
    
	/* we dont' care what the client sends us */
    tcp_write("HTTP/1.0 200 OK\r\n", 17);
	tcp_write("Bogus: FOOBAR\r\n", 15);
	tcp_write("Content-Length: 10\r\n\r\n", 22);
	tcp_write("1234567890", 10);
    tcp_close();
    return 1;
}

#include <tcp.h>

int
main(void)
{
	int i, j;
	char buf[1024];

	tcp_socket();
	tcp_listen((ipaddr_t*) my_ipaddr, 80);

	j = 2;
	tcp_read(buf, j);
	for (i = 0; i < 10; i++) {
	  	j = j << 1;
		tcp_read(buf, j);
	}

	tcp_close();
}

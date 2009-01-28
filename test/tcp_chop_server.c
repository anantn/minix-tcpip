#include <tcp.h>

int
main(void)
{
	int i, j, k;
	char buf[1024];

	tcp_socket();
	tcp_listen(80, (ipaddr_t*) my_ipaddr);

	j = 2;
	k = tcp_read(buf, j);
	printf("Read %d of requested %d bytes\n", k, j);
	for (i = 0; i < 10; i++) {
	  	j = j << 1;
		k = tcp_read(buf, j);
		printf("Read %d of requested %d bytes\n", k, j);
	}

	tcp_close();
	return 1;
}

#include <tcp.h>

int
main(void)
{
	int i, j, k;
	char buf[1024];

	tcp_socket();
	tcp_listen(80, (ipaddr_t*) my_ipaddr);

	j = 2;
	tcp_read(buf, j);
	k = 0;
	for (i = 0; i < 10; i++) {
	  	j = j << 1;
		do {
			k += tcp_read(buf, j - k);
			printf("\nk=%d, j=%d\n", k, j);
		} while (k != j);
	}

	tcp_close();
	return 1;
}

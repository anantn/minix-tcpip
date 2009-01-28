#include <tcp.h>

int
main(void)
{
	int i, j, k;
	char buf[1024];

	tcp_socket();
	tcp_connect(inet_addr("192.168.0.1"), 80);

	memset(buf, 'a', 1024);
	j = 2;
	tcp_write(buf, j);
	k = 0;
	for (i = 0; i < 10; i++) {
	  	memset(buf, 'b' + i, 1024);
	  	j = j << 1;
		while ((k += tcp_write(buf, j - k) != j)) {}
	}

	tcp_close();
	return 1;
}

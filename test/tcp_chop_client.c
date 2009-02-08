#include <tcp.h>

int
main(void)
{
	int i, j, k, l;
	char buf[1024];

	tcp_socket();
	tcp_connect(inet_addr("192.168.0.1"), 80);

	memset(buf, 'a', 1024);
	j = 1024;
	k = tcp_write(buf, j);
	l = k;
	fprintf(stderr, "Written %d of %d requested bytes\n", k, j);
	for (i = 0; i < 10; i++) {
	  	memset(buf, 'b' + i, 1024);
	  	j = j >> 1;
		k = tcp_write(buf, j);
		l += k;
		fprintf(stderr, "Written %d of %d requested bytes\n", k, j);
	}

	fprintf(stderr, "Total written: %d\n", l);
	tcp_close();
	return 1;
}

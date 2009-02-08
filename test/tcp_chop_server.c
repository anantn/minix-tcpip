#include <tcp.h>

int
main(void)
{
	int i, j, k, l;
	char buf[1024];

	tcp_socket();
	tcp_listen(80, (ipaddr_t*) my_ipaddr);

	j = 2;
	k = tcp_read(buf, j);
	l = k;
	fprintf(stderr, "Read %d of requested %d bytes\n", k, j);
	while (l <= 3070) {
	  	j = j << 1;
		k = tcp_read(buf, j);
		l += k;
		if (k == EOF)
			break;
		fprintf(stderr, "Read total %d of requested %d bytes\n", k, j);
	}
	
	fprintf(stderr, "Total read %d\n", l);
	tcp_close();
	return 1;
}

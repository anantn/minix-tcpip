#include "tcp.h"

int
main(int argc, char** argv) {
	int i;
    char* dat;
    u8_t flags;
    ipaddr_t src;
    int len, data_sz;
    u32_t seq_nb, ack_nb;
    u16_t src_port, dst_port, win_sz;
    
    tcp_socket();
    len = recv_tcp_packet(&src, &src_port, &dst_port, &seq_nb,
                            &ack_nb, &flags, &win_sz, dat, &data_sz);

	printf("Received %d bytes!\n", data_sz);
	for (i = 0; i < data_sz; i++) printf("%c", dat[i]);
	printf("\n");

    return 1;
}


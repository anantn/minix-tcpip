CC= cc

CFLAGS = -Wall -DDEBUG -I/usr/local/include/cn -I../ip -I../tcp
LDFLAGS = -L/usr/local/lib -L../ip -L../tcp -ltcp -lip -lcn

all: tcp_actual_chksum tcp_send tcp_recv tcp_connect tcp_listen tcp_connect_parallel

tcp_actual_chksum: tcp_actual_chksum.o
	$(CC) $(CFLAGS) -o tcp_actual_chksum tcp_actual_chksum.o $(LDFLAGS) 
		
tcp_send: tcp_send.o
	$(CC) $(CFLAGS) -o tcp_send tcp_send.o $(LDFLAGS) 

tcp_recv: tcp_recv.o
	$(CC) $(CFLAGS) -o tcp_recv tcp_recv.o $(LDFLAGS) 

tcp_connect: tcp_connect.c
	$(CC) $(CFLAGS) -o tcp_connect tcp_connect.c $(LDFLAGS)

tcp_listen: tcp_listen.c
	$(CC) $(CFLAGS) -o tcp_listen tcp_listen.c $(LDFLAGS)

tcp_connect_parallel: tcp_connect_parallel.c
	$(CC) $(CFLAGS) -o tcp_connect_parallel tcp_connect_parallel.c $(LDFLAGS)

clean:
	rm -f *.o tcp_actual_chksum tcp_send tcp_recv a.out tcp_connect tcp_listen tcp_connect_parallel


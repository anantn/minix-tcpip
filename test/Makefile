CC= cc

CFLAGS = -Wall -DDEBUG -I/usr/local/include/cn -I../ip -I../tcp
LDFLAGS = -L/usr/local/lib -L../ip -L../tcp -ltcp -lip -lcn

all: tcp_actual_chksum tcp_send tcp_recv

tcp_actual_chksum: tcp_actual_chksum.o
	$(CC) $(CFLAGS) -o tcp_actual_chksum tcp_actual_chksum.o $(LDFLAGS) 
		
tcp_send: tcp_send.o
	$(CC) $(CFLAGS) -o tcp_send tcp_send.o $(LDFLAGS) 

tcp_recv: tcp_recv.o
	$(CC) $(CFLAGS) -o tcp_recv tcp_recv.o $(LDFLAGS) 

clean:
	rm -f *.o tcp_actual_chksum tcp_send tcp_recv a.out


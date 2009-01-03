AR = aal
RANLIB = touch

CFLAGS  = -Wall -I/usr/local/include/cn
LDFLAGS = -L. -L/usr/local/lib -lip -lcn
SOURCES = udp.c ip.c arp.c
OBJECTS = udp.o ip.o arp.o

all: libip.a client server

libip.a: $(OBJECTS)
	$(AR) rv libip.a $(OBJECTS)
	$(RANLIB) libip.a

client: client.o libip.a
	$(CC) $(CFLAGS) -o client client.o $(LDFLAGS)

server: server.o libip.a
	$(CC) $(CFLAGS) -o server server.o $(LDFLAGS)

clean:
	rm -f $(OBJECTS) libip.a a.out *.bak core errs client server client.o server.o


depend:
	makedepend $(CFLAGS) $(SOURCES)



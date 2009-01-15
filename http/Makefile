CC= cc

CFLAGS = -Wall -DDEBUG -I/usr/local/include/cn -I../ip -I../tcp
LDFLAGS = -L/usr/local/lib -L../ip -L../tcp -ltcp -lip -lcn

all: httpc httpd

httpc: httpc.c
	$(CC) $(CFLAGS) -o httpc httpc.c $(LDFLAGS) 
		
httpd: httpd.c
	$(CC) $(CFLAGS) -o httpd httpd.c $(LDFLAGS) 

clean:
	rm -f httpc httpd


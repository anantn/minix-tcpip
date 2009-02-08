
CC= cc
CFLAGS = -Wall -DDEBUG=$(DEBUG) -I. -I/usr/local/include/cn -I../ip -I../tcp
LDFLAGS = -L/usr/local/lib -L../ip -L../tcp -ltcp -lip -lcn

all: httpc httpd

httpc: httpc.c http.c
	$(CC) $(CFLAGS) -o httpc httpc.c http.c $(LDFLAGS) 
		
httpd: httpd.c
	$(CC) $(CFLAGS) -o httpd httpd.c http.c $(LDFLAGS) 

clean:
	rm -f httpc httpd


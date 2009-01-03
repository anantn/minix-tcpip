CC = cc
AR = aal
RANLIB = touch

CFLAGS = -Wall -I/usr/local/include/cn -I../ip
ifdef DEBUG
  CFLAGS += -DDEBUG
endif
LDFLAGS = -L../ip -L/usr/local/lib -lip -lcn

SOURCES = tcp.c util.c
OBJECTS = tcp.o util.o

all: libtcp.a

libtcp.a: $(OBJECTS)
	$(AR) rv libtcp.a $(OBJECTS)
	$(RANLIB) libtcp.a

clean:
	rm -f $(OBJECTS) libtcp.a


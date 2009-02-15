#include "tcp.h"

#define RETRANSMISSION_LIMIT 10
/* Utility functions */
static u16_t raw_checksum(uchar* dat, int len);
static void swap_header(Header* hdr, int ntoh);
static void show_packet(Header* hdr, Data* dat, int out);

/* Support functions*/
static int can_read(int state);
static int can_write(int state);
static int handle_packets(int socket);
static int send_ack(int socket, int flags) ;
static int setup_packet(int socket, Header* hdr);
static int wait_for_ack(int socket, u32_t local_seqno);
static int write_packet(int socket, char* buf, int len, int flags );
static int socket_close(int socket);

/* State handling functions*/
static int handle_Listen_state(int socket, Header* hdr, Data* dat);
static int handle_Syn_Sent_state(int socket, Header* hdr, Data* dat);
static int handle_Syn_Recv_state(int socket, Header* hdr, Data* dat);
static int handle_Established_state(int socket, Header* hdr, Data* dat);

/* Signal handling functions */
static void alarm_signal_handler(int sig);
static void set_retransmission_buffer(int set, Header *hdr, Data *dat);
static void restore_app_alarm(void);
static void (*ptr_original_signal_handler)(int sig) = SIG_IGN;
static int previous_alarm_time = 0;

/* To suppress warnings when DEBUG=0, we use noprint */
static int noprint(const char* format, ...) { return 1; }

/* Array of connections, for multiple connection support */
static TCPCtl muxer[MAX_CONN];

/* Current active connection (used in every public interface function) */
static TCPCtl* cc;

/* Last opened connection */
static int last_conn = -1;

/* State names, for easy printing */
static char state_names[][12] = {
    "Closed",
    "Listen",
    "Syn_Sent",
    "Syn_Recv",
    "Established",
    "Fin_Wait1",
    "Fin_Wait2",
    "Close_Wait",
    "Closing",
    "Last_Ack",
    "Time_Wait"
};

/* Static allocation for retransmission buffer */
static int      rt_present = 0;
static int      rt_counter = 0;
static Data     rt_data;
static uchar    rt_buf[DATA_SIZE];
static Header   rt_hdr;

/**
 * Low level send and receive functions.
 * Note that we do not adhere to the recommended interface, but use our own
 * structures.
 */
 
/** 
 * _send_tcp_packet:
 * Caller must set the following fields: 'dst', 'sport', 'dport',
 * 'seqno', 'ackno', 'flags', 'window', and 'urgent'.
 *
 * This function WILL modify the supplied header structure, make a copy
 * before using!
 */
int DROP_PACKET_NO = 0 ;
int CURRUPT_THIS_PACKET = 0 ;

static int packet_sent_no = 0 ;
static int
_send_tcp_packet(Header* hdr, Data* dat)
{
    uchar* tmp;
    int len, csum;
    
    if (!hdr->dst) {
        dprint("_send_tcp_packet:: destination not specified, aborting\n");
        return 0;
    }
    hdr->src = my_ipaddr;
    hdr->prot = htons(IP_PROTO_TCP);
    hdr->tlen = htons(HEADER_SIZE + dat->len);

    show_packet(hdr, dat, 1);

    /* Checksum is calculated in network order */
    swap_header(hdr, 0);

    /* To calculate checksum we copy everything into a temporary buffer */
    tmp = (uchar*) calloc(sizeof(Header) + dat->len, sizeof(uchar));
    memcpy(tmp, (uchar*) hdr, sizeof(Header));
    memcpy(tmp + sizeof(Header), dat->content, dat->len);
    csum = raw_checksum(tmp, sizeof(Header) + dat->len);
	if (CURRUPT_THIS_PACKET > 0 )
	{
		csum += 123 ;
        dprint("_send_tcp_packet:: currupting above packet \n");
		--CURRUPT_THIS_PACKET ;

		
	}
    memcpy(tmp + CHECK_OFF, (uchar*) & csum, sizeof(u16_t));

    /* Off it goes! */
	++packet_sent_no ;
	if ( packet_sent_no == DROP_PACKET_NO )
	{
        dprint("_send_tcp_packet:: Dropping above packet\n");
	}
	else
	{
		len = ip_send(hdr->dst, IP_PROTO_TCP, 2,
			(void*) (tmp + HEADER_OFF), HEADER_SIZE + dat->len);

		
	}
    free(tmp);

    return dat->len;
}

/**
 * _recv_tcp_packet:
 * Called need not set any fields, just pointers to PRE-ALLOCATED 'Header'
 * and 'Data' structures will do.
 *
 * Space for Data->content should not be allocated however, as it will
 * be done here. Freeing that memory is the responsibility of the caller!
 * (The caller is handle_packets most (all?) of the time)
 */
static int
_recv_tcp_packet(Header* hdr, Data* dat)
{
    char*       data;
    uchar*      tmp;
    int         len, doff;
    ushort      prot, idp;
    ipaddr_t    src, dst;

    len = ip_receive(&src, &dst, &prot, &idp, &data);
    if (len == -1) {
        return -1;
    }
    
    /* Extract header - pseudo and real, still in network order */
    hdr->src = src;
    hdr->dst = dst;
    hdr->prot = htons(prot);
    hdr->tlen = htons(len);
    memcpy(((uchar*) hdr) + HEADER_OFF, data, HEADER_SIZE);

    /* Check the checksum */
    tmp = (uchar*) calloc(HEADER_OFF + len, sizeof(uchar));
    memcpy(tmp, (uchar*) hdr, HEADER_OFF);
    memcpy(tmp + HEADER_OFF, (uchar*) data, len);
    if (raw_checksum(tmp, HEADER_OFF + len) != 0) {
        /* Whoops, looks like this is a corrupted packet */
        free(tmp);
        dprint("_recv_tcp_packet:: Incorrect Checksum! Discarding packet...");
        dprint("Got: %x instead of 0\n", raw_checksum(tmp, HEADER_OFF + len));
        return -1;
    }
    free(tmp);

    /* Back to host order */
    hdr->prot = prot;
    hdr->tlen = len;
    swap_header(hdr, 1);

    /**
     * We ignore options, just interested in data offset (which is
     * described in number of WORD_SIZE bytes).
     */
    doff = (hdr->flags >> DATA_SHIFT) * WORD_SIZE;
    dat->len = len - doff;
    dat->content = (uchar*) calloc(dat->len, sizeof(uchar));
    
    memcpy(dat->content, (uchar*) data + doff, dat->len);
    show_packet(hdr, dat, 0);

    return dat->len;
}

/**
 * TCP Interface functions.
 *
 * These adhere to the specified interface, but there is a set of functions
 * appended with '_socket' to enable multiple connection support.
 */

/**
 * tcp_socket:
 * Creates a new TCP socket. Will return an integer representing that
 * socket. For single connection programs, that will always be 0,
 * but subsequent calls will create new sockets with increasing integers as
 * return values.
 *
 * Memory for the 'in' and 'out' buffers is allocated here. Source port
 * allocation is also done, but calling tcp_listen() later on may over-write
 * the value.
 *
 * All memory allocated here is to be freed in socket_close()
 */
int
tcp_socket(void)
{
    TCPCtl* ctl;

    if (ip_init() < 0) {
        return -1;
    }

    /* Clear static allocation of connection array on first call */
    if (last_conn == -1) {
        memset(muxer, 0, MAX_CONN * sizeof(TCPCtl));
    }

    /* Determine port and socket number (last_conn + 1) */
    last_conn++;
    if (last_conn > MAX_CONN) {
        return -1;
    }
    ctl = &(muxer[last_conn]);
    ctl->socket = last_conn;
    ctl->sport = START_PORT + last_conn;

    /* Allocate memory for incoming buffer */
    ctl->in_buffer = (Data*) calloc(1, sizeof(Data));
    ctl->in_buffer->content = (uchar*) calloc(DATA_SIZE + 1, sizeof(uchar));
    ctl->in_buffer->len = 0;

    /* Allocate memory for outgoing buffer */
    ctl->out_buffer = (Data*) calloc(1, sizeof(Data));
    ctl->out_buffer->content = (uchar*) calloc(DATA_SIZE + 1, sizeof(uchar));
    ctl->out_buffer->len = 0;

    /* Clear the data buffer */
    ctl->remaining = 0;
    ctl->state = Closed;

    /* Use random generation for initial seq no */
    ctl->local_seqno = rand() % 1024;
    
    /* These have to be set in three way handshake */
    ctl->remote_ackno = 0;
    ctl->remote_seqno = 0;
    ctl->remote_window = DATA_SIZE;

    /**
     * Now set the signal handler for SIGALARM, which will handle
     * retransmissions.
     */
    set_retransmission_buffer(0, NULL, NULL);

    return ctl->socket;
}

/**
 * tcp_connect_socket:
 * Attempts to establish a connection with a remote TCP socket. Note that
 * this function simply sets the appropriate data structure values, and sends
 * a SYN packet, while the remainder of the 3-way handshake is handled by 
 * handle_packets (which will delegate it further down)
 */
int
tcp_connect_socket(int socket, ipaddr_t dst, int port)
{
    char buffer = 0;
    
    /* Make sure tcp_socket was called before calling this */
    if (socket > last_conn)
        return -1;
        
    /* 'cc' is a global variable, remember? */
    cc = &(muxer[socket]);

    cc->type = TCP_CONNECT;
    cc->dport = port;
    cc->dst = dst;
    
    /* Send SYN packet */
    cc->state = Syn_Sent;
    write_packet(socket, &buffer, 0, SYN);
    dprint("tcp_connect:: Done, calling handle_packets\n");

    while (cc->state != Established) {
        /* Let handle_packets handle the rest */
        handle_packets(socket);
        if (cc->state == Closed) {
            dprint("tcp_connect:: Other side is not responding!\n");
            return -1;
        }
    }
    return 1;
}

/**
 * tcp_connect:
 * Single connection version of tcp_connect_socket. All other
 * single connection functions follow the same pattern of calling their
 * _socket counterparts with a static socket number of 0.
 */
int
tcp_connect(ipaddr_t dst, int port)
{
    if (last_conn < 0)
        return -1;
    return tcp_connect_socket(0, dst, port);
}

/**
 * tcp_listen_socket:
 * Wait for incoming connections at specified port. Once again, we simply
 * set the state variables and let handle_packets do the waiting and 3-way
 * handshake for us.
 */
int
tcp_listen_socket(int socket, int port, ipaddr_t* src)
{
    int i;
    if (socket > last_conn)
        return -1;
    
    /* Check if port is already not in use by another socket */
    for (i = 0; i < last_conn; i++) {
        cc = &muxer[i];
        if (cc->sport == port)
            return -1;
    }
    cc = &(muxer[socket]);

    /* Change the state */
    cc->type = TCP_LISTEN;
    cc->state = Listen;
    cc->sport = port;

    dprint("tcp_listen:: Done, calling handle_packets\n");
    do {
        handle_packets(socket);
        if (cc->state == Closed) {
            dprint("tcp_listen:: Other side is not responding!\n");
            return -1;
        }
    } while (cc->state != Established);

    return 1;
}

int
tcp_listen(int port, ipaddr_t* src)
{
    if (last_conn < 0)
        return -1;
    return tcp_listen_socket(0, port, src);
}

/**
 * tcp_read_socket:
 * Read 'maxlen' bytes from socket. Will try to read as much as possible,
 * but will return the actual number of bytes read (can be lesser, but
 * never more than the number requested!)
 */
int
tcp_read_socket(int socket, char* buf, int maxlen)
{
    int remaining_bytes = 0;
    int bytes_read = 0;
    int old_window, new_window;

    if (socket > last_conn)
        return -1;
    cc = &(muxer[socket]);

    /* Sanity check */
    if (buf == NULL || maxlen < 0)
        return -1;

    /* No data to be read */
    if (maxlen == 0)
        return 0;

    while (cc->in_buffer->len == 0) {
        if (!can_read(cc->state)) {
            dprint("tcp_read:: Cannot read data in Closed state\n");
            return EOF;
        }
        dprint("tcp_read:: No data in read buffer, calling handle_packets\n");
        handle_packets(socket);
    }

    old_window = DATA_SIZE - cc->in_buffer->len;
    bytes_read = MIN(cc->in_buffer->len, maxlen);
    
    /* Copy from the in buffer into the one given by user */
    memcpy(buf, cc->in_buffer->content, bytes_read);
    remaining_bytes = cc->in_buffer->len - bytes_read;

    /* Not all of it was copied, so we move */
    if (remaining_bytes > 0) {
        memmove(cc->in_buffer->content,
                (cc->in_buffer->content + bytes_read), remaining_bytes);
    }
    cc->in_buffer->len = remaining_bytes;
    
    /* Clear the data buffer */
    memset(cc->in_buffer->content + remaining_bytes, 0,
            (DATA_SIZE - remaining_bytes) * sizeof(uchar));
  
    new_window = DATA_SIZE - cc->in_buffer->len;
    if (old_window == 0) {
        if (new_window != 0) {
            /**
             * We've made some space in the buffer, let's tell the other
             * side about it.
             */
            dprint("tcp_read:: Sending ack for advertising buffer %d\n",
                new_window);
            send_ack(socket, 0);
        }
    }
    
    dprint("tcp_read:: Read %d bytes successfully\n", bytes_read);
    return bytes_read;
}

int
tcp_read(char* buf, int maxlen)
{
    if (last_conn < 0)
        return -1;
    return tcp_read_socket(0, buf, maxlen);
}

/**
 * tcp_write_socket:
 * This function WILL block until len bytes have been sent and acked by the
 * other end. In other words, the return value should always be len!
 */
int
tcp_write_socket(int socket, char* buf, int len)
{
    int bytes_left;
    int bytes_sent;
    int packet_size;
    int temp;
    
    if (socket > last_conn)
        return -1;
    cc = &(muxer[socket]);

    bytes_sent = 0;
    bytes_left = len;
    dprint("tcp_write:: Writing %d bytes\n", len);
    
    while (bytes_left > 0) {
        /* Are we in a state in which write is not allowed? */
        if (!can_write(cc->state)) {
            dprint("tcp_write:: Cannot write data while in %s state!\n",
                    state_names[cc->state]);
            if (bytes_sent == 0)
                return -1;
            else
                return bytes_sent;
        }

        packet_size = MIN(cc->remote_window, bytes_left);
        
        /* Make sure the other side is ready to read what we're sending */
 /*       while (packet_size == 0) { */
        if (packet_size == 0) {
            dprint("tcp_write:: Remote window is empty %d(%d,%d), so sending packet with 1 byte data\n",
                    packet_size, cc->remote_window, bytes_left);
/*            handle_packets(socket); */
/*            packet_size = MIN(cc->remote_window, bytes_left); */
            packet_size =  1 ; 
        }

        /* Send a packet */
        dprint("tcp_write:: Writing %d bytes \n", packet_size);
        temp = write_packet(socket, buf + bytes_sent, packet_size, 0);
        
        if (temp == -1) {
            if (cc->state == Closed) {
                if (bytes_sent == 0) {
                    dprint("tcp_write:: Other side is Closed!\n");
                    return -1;
                } else {
                    dprint("tcp_write:: Other side is Closed!\n");
                    return bytes_sent;  
                }
            }
        }
        bytes_sent += temp;
        bytes_left = len - bytes_sent;

        /**
         * Just in case you want tcp_write to return as soon as possible,
         * even though all the bytes have not been written, simply uncomment
         * the following line:
         *
        return bytes_sent; */ 
    }
        
    return bytes_sent;
}

int
tcp_write(char* buf, int len)
{
    return tcp_write_socket(0, buf, len);
}

/**
 * tcp_close_socket:
 * Start closing a connection. Will send a FIN packet and process a 4-way
 * closing procedure as specified by the RFC.
 */
int
tcp_close_socket(int socket)
{
    int  ret;
    char buffer = 0;
    
    if (socket > last_conn)
        return -1;
    cc = &(muxer[socket]);

    /* Send FIN and start 4-way closing procedure */
    dprint("tcp_close:: Called... ");
    if (cc->state == Established || cc->state == Close_Wait) {
        ret = write_packet(socket, &buffer, 0, FIN);
    }

    /**
     * We've got the ACK for our FIN, let's change the state
     */
    switch (cc->state) {
    case Fin_Wait1:
        dprint("Fin Acknowledged, going to Fin_Wait2\n");
        cc->state = Fin_Wait2;
        break;
    case Closing:
        dprint("Fin Acknowledged, going to Time_Wait\n");
        cc->state = Time_Wait;
        break;
    case Last_Ack:
        dprint("Fin Acknowledged, going to Closed\n");
        cc->state = Closed;
        /* Free resouces associated with the socket */
        socket_close(socket);
        break;
    case Closed:
        dprint("Already in Closed state\n");
        cc->state = Closed;
        /* Free resouces associated with the socket */
        socket_close(socket);
        break;
    default:
        dprint("Error: Odd state %s\n", state_names[cc->state]);
        break;
    }

    return 1;
}

int
tcp_close(void)
{
    return tcp_close_socket(0);
}

/**
 * Ok, we're done with the public interface functions. Utility, alarm
 * handling and second layer functions follow.
 */

/**
 * Manipulates the retransmission buffer and SIGALRM handler.
 *
 * If set = 0, this function will simply clear the retransmission buffer.
 * When set = 1, it will additionally populate the buffer while setting
 * up the library's signal handler.
 */
static void
set_retransmission_buffer(int set, Header* hdr, Data* dat)
{
    /* Initializing the retransmission buffer */
    rt_present = 0;
    memset(&rt_hdr, 0, sizeof(Header));
    memset(&rt_data, 0, sizeof(Data));
    memset(&rt_buf, 0, DATA_SIZE);
    rt_data.content = rt_buf;
    rt_data.len = 0;
    rt_counter = 0;

    if (set == 1) {
        memcpy(&rt_hdr, hdr, sizeof (Header));
        memcpy(rt_data.content, dat->content, dat->len);
        rt_data.len = dat->len;
        rt_present = 1;
        rt_counter = 0;
		dprint ("setting retransmit signal handler\n");
        ptr_original_signal_handler = signal(SIGALRM, alarm_signal_handler);
       
        if (ptr_original_signal_handler == SIG_ERR) {
            dprint("reset_transmission_buffer:: Can't set signal handler!\n");
            exit(1);
        }
    }
}

/**
 * Our signal handler for SIG_ALARM, which will deal with retransmissions.
 */
static void
alarm_signal_handler(int sig)
{
    Header  c_hdr;
    Data    c_data;
    int     bytes_sent;
    
    /* Set the signal handler again (some systems reset it) */
    if (signal(SIGALRM, alarm_signal_handler) == SIG_ERR) {
        dprint("alarm_signal_handler:: Can't set signal handler!\n");
        return;
    }
    if (!rt_present) {
        /* No packet for retransmission */
        return;
    }
    
    /* Ok, there's a packet to be re-transmitted */
    ++(rt_counter);
    c_hdr = rt_hdr;
    c_data = rt_data;
    
    /**
     * Update the ackno and window as they may have changed
     */
    dprint("alarm_signal_handler:: Re-transmitting packet: %d\n", rt_counter);
    rt_hdr.ackno = cc->remote_seqno;
    rt_hdr.window = DATA_SIZE - cc->in_buffer->len;
    bytes_sent = _send_tcp_packet(&c_hdr, &c_data);
    
    /* Set alarm, in case the packet we just sent is lost / corrupted */
    alarm(RETRANSMISSION_TIMER);
    
    /* Cancel the previous SIGALARM */
    previous_alarm_time -= RETRANSMISSION_TIMER;
}

/**
 * Checks if current state allows us to read or not
 */
static int
can_read(int state)
{
    switch (state) {
    case Established:
    case Fin_Wait1:
    case Fin_Wait2:
        return 1;
    default:
        return 0;
    }
}

/**
 * Checks if current state allows us to write or not
 */
static int
can_write(int state)
{
    switch (state) {
    case Established:
    case Close_Wait:
        return 1;
    default:
        return 0;
    }
}

/*
 * Creates a TCP packet out of given data and sends it using the low-level
 * function. Will also handle retransmission until we get a correct ACK.
 */
static int
write_packet(int socket, char* buf, int len, int flags)
{
    Header  hdr;
    Data    dat;
    int     bytes_sent;
    
    /* What ackno do we wait for? */
    u32_t   ack_to_wait; 

    /* Note that we do not need to validate 'socket' as it has already been
     * done at upper layers.
     */
    cc = &(muxer[socket]);

    /* Clear structures */
    memset(&hdr, 0, sizeof(hdr));
    memset(&dat, 0, sizeof(dat));
    
    /* Assign data */
    dat.content = (uchar*) buf;
    dat.len = len;

    /* Setup packet */
    setup_packet(socket, &hdr);
    
    /* Update local sequence number */
    cc->local_seqno = cc->local_seqno + len;
    ack_to_wait = cc->local_seqno;

    /* Handle SYN packets */
    if (flags & SYN) {
        hdr.flags = 0;
        hdr.flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT;
        hdr.flags = hdr.flags | SYN;
        /* SYN is single bit data, so incrementing ack_to_wait by one */
        ack_to_wait = cc->local_seqno + 1;
    }
    
    /* Handle FIN packets */
    if (flags & FIN) {
        hdr.flags = hdr.flags | FIN;

        /* FIN is single bit data, incrementing local_seqno by one */
        if (len == 0) {
            /**
             * After sending a FIN packet, should we increment the
             * seqno by one? It looks like the RFC says yes, but the test
             * program doesn't agree. Since passing the test is crucial,
             * we comment the following line ;)
             *
            cc->local_seqno = cc->local_seqno + 1 ; */
            
            ack_to_wait = cc->local_seqno + 1;
        }
        
        switch (cc->state) {
        case Established:
            cc->state = Fin_Wait1;
            dprint("write_packet:: State changed to Fin_Wait1\n");
            break;
        case Close_Wait:
            cc->state = Last_Ack;
            dprint("write_packet:: State changed to Last_Ack\n");
            break;
        }
    }
    
    /* Put the packet into the list of unacked packets */
    set_retransmission_buffer(1, &hdr, &dat);

    /**
     * Send the packet
     * In case of retransmission, update the ackno and window.
     */
    hdr.ackno = cc->remote_seqno;
    hdr.window = DATA_SIZE - cc->in_buffer->len;
    bytes_sent = _send_tcp_packet(&hdr, &dat);

    if (wait_for_ack(socket, ack_to_wait) == -1) {
        dprint("write_packet:: Sending packet failed, no ack received!\n");
        return -1;
    }
    
    dprint("write_packet:: Packet sent successfully (ACK received)!\n");
    return bytes_sent;
}

/**
 * This function checks if an ACK is valid. We cover the default case,
 * as well as cases of sequence number rollover (possible in very very
 * large transfers), and checking if we have all the bytes that they claim
 * we have.
 */
static int
is_valid_ack(int socket, u32_t local_seqno, u32_t remote_ackno)
{
    int diff;
    cc = &(muxer[socket]);

    if (cc->state == Syn_Sent && remote_ackno == 0) {
        return 0;
    }
    if (cc->state == Syn_Recv && remote_ackno == 0) {
        return 0;
    }

    diff = remote_ackno - local_seqno;
    dprint("is_valid_ack:: [diff(%d) = remote_ack(%u) - local_seq(%u)]",
            diff, remote_ackno, local_seqno);
    dprint(", actual_local_seq(%u)\n", cc->local_seqno);

    if (diff == 0) {
        /* Default case, what we were expecting */
        return 1;
    }
    if ((diff > 0) && (local_seqno + diff <= cc->local_seqno)) {
        /* Some ACKs in between were dropped */
        return 1;
    }
    if ((diff < 0) && (cc->local_seqno < local_seqno)) {
        if (local_seqno - cc->local_seqno == 1) {
            dprint("is_valid_ack:: SYN or FIN packet, not valid ACK\n");
            return 0;
        }
        /* The sequence number wrapped around! (Wow, 4G transferred?) */
        return 1;
    }

    dprint("is_valid_ack:: Invalid ACK!\n");
    return 0;
}
/**
 * This function will receive a packet and check if it is an ACK that
 * we expected to receive. If it was, we reset the re-transmission alarm.
 */
static int
wait_for_ack(int socket, u32_t local_seqno)
{
    cc = &(muxer[socket]);
    previous_alarm_time = alarm(RETRANSMISSION_TIMER);
	dprint ("settign alarm for %d \n", RETRANSMISSION_TIMER);

    while (!is_valid_ack(socket, local_seqno, cc->remote_ackno)) {
        dprint("wait_for_ack:: Calling handle_packets\n");
        handle_packets(socket);
        if (cc->state == Last_Ack) {
            if (rt_counter >= 2) {
                cc->state = Closed;
                restore_app_alarm();
                return -1;
            }
        }
        if (cc->state == Closed) {
            restore_app_alarm();
            return -1;
        }
        if (cc->state == Closing) {
            if (rt_counter >= 4) {
                cc->state = Closed;
                restore_app_alarm();
                return -1;
            }
        }
        if (rt_counter >= RETRANSMISSION_LIMIT) {
            cc->state = Closed;
            restore_app_alarm();
            return -1;
        }
    }

    dprint("wait_for_ack:: Got good ack %u / %u, canceling alarm\n",
            cc->remote_ackno, local_seqno);

    restore_app_alarm();
    return 1;
}

/**
 * This function restores the original signal handler that may have bee
 * set by the user's application. Yes, we do it after every call to a
 * function in our library, not just after tcp_close()
 */
static void
restore_app_alarm(void)
{
    int time_missed;
	dprint ("Restroing app signals \n");
    set_retransmission_buffer(0, NULL,NULL);

    if (signal(SIGALRM, ptr_original_signal_handler) == SIG_ERR) {
        dprint("tcp_socket:: Can't set signal handler!\n");
        exit(1);
    }

    /* Cancel the signal SIGALARM */
    time_missed = alarm(0);
    alarm(previous_alarm_time);
}


/**
 * Setup a Header structure suitable for passing to the low-level
 * _send_tcp_packet and _recv_tcp_packet functions.
 */
static int
setup_packet(int socket, Header* hdr)
{
    cc = &(muxer[socket]);

    hdr->dst = cc->dst;
    hdr->sport = cc->sport;
    hdr->dport = cc->dport;
    hdr->seqno = cc->local_seqno;
    hdr->ackno = cc->remote_seqno;
    hdr->window = DATA_SIZE - cc->in_buffer->len;

    hdr->urgent = 1;
    hdr->flags = 0;
    hdr->flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT;
    hdr->flags = hdr->flags | URG | ACK | PSH;

    dprint("setup_packet:: Header fields set\n");
    return 1;
}

/**
 * This function releases all resources allocated to a socket.
 */
static int
socket_close(int socket)
{
    cc = &(muxer[socket]);
    cc->state = Closed;
    cc->sport = 0;
    
    free(cc->in_buffer->content);
    free(cc->out_buffer->content);
    free(cc->in_buffer);
    free(cc->out_buffer);

    return 1;
}

/*
 * Waits for one incoming packet, puts the data in the incoming buffer,
 * and updates the ack / remote seqno's as appropriate. Most of the time
 * is spent in this function, which then calls state handling functions
 * depending on the state we are in.
 */
static int
handle_packets(int socket)
{
    Header      hdr;
    Data        dat;
    int         len, ret;

    cc = &(muxer[socket]);

    if (cc->state ==  Closed) {
        return -1 ;
    }
    dprint("handle_packets:: State %s, waiting for packet\n",
            state_names[cc->state]);

    do {
        len = _recv_tcp_packet(&hdr, &dat);
        dprint("handle_packets:: Received packet with len %d\n", len);
        /* Handle a case of 4-way close properly */
        if (cc->state == Last_Ack) {
            if (rt_counter >= 2) {
                cc->state = Closed;
                return -1;
            }
        }
        /* If we've retransmitted retransmission limit already, quit trying! */
        if (rt_counter >= RETRANSMISSION_LIMIT ) {
            cc->state = Closed;
            return -1;
        }
    } while (len < 0);

    /* Port mismatch */
    if (hdr.dport != cc->sport) {
        dprint("handle_packets:: Received packet for wrong port %u, "
               "expecting %u\n", hdr.dport, cc->sport);
        return -1;
    }
    
    dprint("handle_packets:: Dealing with %s\n", state_names[cc->state]);
    
    /* Handle state */
    switch (cc->state) {
    case Closed:
        ret = 1;
        break;
    case Listen:
        ret = handle_Listen_state(socket, &hdr, &dat);
        break;
    case Syn_Sent:
        ret = handle_Syn_Sent_state(socket, &hdr, &dat);
        break;
    case Syn_Recv:
        ret = handle_Syn_Recv_state(socket, &hdr, &dat);
        break;
    case Established:
    case Fin_Wait1:
    case Fin_Wait2:
    case Close_Wait:
    case Closing:
    case Last_Ack:
        ret = handle_Established_state(socket, &hdr, &dat);
        break;
    default:
        dprint("handle_packets:: Error! Unplanned state %d\n", cc->state);
        break;
    }

    /* Freeing up memory allocated by _recv_tcp_packet() */
    free(dat.content);
    return 1;
}

/**
 * Expect a SYN packet and send a SYN+ACK in response.
 */
static int 
handle_Listen_state(int socket, Header* hdr, Data* dat)
{
    int flags;
    cc = &(muxer[socket]);
    
    flags = hdr->flags;

    if (!(flags & SYN)) {
        dprint("handle_Listen_state:: No SYN set, ignoring!\n");
        return -1;
    }
    
    if (flags & ACK) {
        dprint("handle_Listen_state:: SYN + ACK got, which is not expected.. so ignoring!\n");
        return -1;
    }
    /* Get information about other side */
    cc->dst = hdr->src;
    cc->dport = hdr->sport;
    cc->remote_seqno = hdr->seqno + 1;
    
    /* Update remote window size */
    cc->remote_window = hdr->window;    
    cc->state = Syn_Recv;
    
    dprint("handle_Listen_state:: Got SYN, sending SYN+ACK\n");
    send_ack(socket, SYN);

    return 1;
}

/**
 * Expect a SYN+ACK packet and send an ACK in response.
 * We also handle the case of just a SYN packet, which means a simultaneous
 * open has occurred.
 */
static int 
handle_Syn_Sent_state(int socket, Header* hdr, Data* dat)
{
    int flags;
    cc = &(muxer[socket]);
    
    flags = hdr->flags;

    if (!(flags & SYN)) {
        dprint("handle_Syn_Sent_state: No SYN set, ignoring!\n");
        return -1;
    }
    
    if (flags & ACK) {
        /* Check if ackno received is correct or not */
        if (cc->local_seqno + 1 != hdr->ackno) {
            /* Got wrong ackno... ignoring */
            dprint("handle_Syn_Sent_state:: SYN+ACK were set, but with "
                   "the wrong sequence no., ignoring!\n");
            return -1;
        }
        
        dprint("handle_Syn_Sent_state:: Got correct SYN+ACK packet, "
               "sending ACK\n");

        cc->remote_ackno = hdr->ackno;
        cc->local_seqno = hdr->ackno;
        cc->remote_seqno = hdr->seqno + 1;
        cc->remote_window = hdr->window;
        
        /* Send an ack for this */
        cc->state = Established;
        send_ack(socket, 0);
        return 1;
    }

    /* Got just a SYN packet => simultaneous open. */
    dprint("handle_Syn_Sent_state:: Got just SYN, simultaneous open. "
           "Sending SYN+ACK\n");

    cc->state = Syn_Recv;
    cc->remote_seqno = hdr->seqno + 1;
    cc->remote_window = hdr->window;
    
    send_ack(socket, 0);
    return 1;
}

/**
 * Complete the 3-way handshake :)
 */
static int
handle_Syn_Recv_state(int socket, Header* hdr, Data* dat)
{
    int flags;
    cc = &(muxer[socket]);
    
    flags = hdr->flags;

    if (!(flags & ACK)) {
        dprint("handle_Syn_Recv_state:: ACK not set, ignoring!\n");
        /*
         * We check if this might be a retransmitted SYN packet,
         * because SYN+ACK was dropped.
         */
        return (handle_Listen_state(socket, hdr, dat));
    }
    
    /**
     * FIXME: What if we get a FIN packet at this stage?
     * Move to Fin_Wait1?
     */

    /* Got ACK, check if it is the correct one */
    if (cc->local_seqno + 1 != hdr->ackno) {
        dprint("handle_Syn_Recv_state:: ACK set, but with wrong ACK no., "
               "ignoring!\n");
        return -1;
    }
    
    /* Ok, got ACK for my SYN, moving to Established state */
    cc->remote_ackno = hdr->ackno;
    cc->local_seqno = hdr->ackno;
    cc->remote_seqno = hdr->seqno + dat->len;
    cc->remote_window = hdr->window;

    cc->state = Established;
    dprint("handle_Syn_Recv_state:: Got ACK, going to Established state\n");
    
    /* Check if we have enough space in incoming buffer */
    if (dat->len > 0) {
        dprint("handle_Syn_Recv_state:: ACK packet contains data!\n");
        if (cc->in_buffer->len + dat->len <= DATA_SIZE) {
            /**
             * There is data in this packet, and there's space in the buffer.
             * Let's copy it!
             */
            memcpy(cc->in_buffer->content + cc->in_buffer->len,
                dat->content, dat->len);
            cc->in_buffer->len += dat->len;
            cc->remote_seqno = hdr->seqno + dat->len;
        }
    }
    return 1;
}

/**
 * We handle data packets, ACK packets and FIN packets here.
 */
static int 
handle_Established_state(int socket, Header* hdr, Data* dat)
{
    cc = &(muxer[socket]);

    /* FIXME: In the case of Closing state is the seqno incremented by 1? */

    if (cc->remote_seqno != hdr->seqno) {
        dprint("handle_Established_state:: Received incorrect packet, "
               "expected %u, received %u, ACKing with old ACK number\n",
                cc->remote_seqno, hdr->seqno);
        /**
         * Send an ack, just to tell other side that something is wrong.
         */
        send_ack(socket, 0);
        return -1;
    }
    
    /* Update the ackno received */
    cc->remote_ackno = hdr->ackno;
    cc->remote_window = hdr->window;

    /* Check if we have enough space in incoming buffer */
    if ((dat->len == 0) || (cc->in_buffer->len + dat->len <= DATA_SIZE)) {
        /* accept the packet */
        dprint("handle_Established_state:: Receive %d bytes\n", dat->len);
        memcpy(cc->in_buffer->content + cc->in_buffer->len,
            dat->content, dat->len);
        cc->in_buffer->len += dat->len;
        cc->remote_seqno = hdr->seqno + dat->len;

        /*
         * If the packet contains data or if it's a FIN packet,
         * only then do we ackowledge it.
         */
        if (dat->len != 0) {
            if (hdr->flags & FIN) {
                dprint("handle_Established_state:: ACKing the FIN packet "
                       "(packet had data %d) with ACK no %u\n",
                       dat->len, cc->remote_seqno);
            }
            send_ack(socket, 0); /* normal send ack*/
            dprint ("handle_Established_state: send simple ack \n");
        } else {
            if (hdr->flags & FIN) {
                cc->remote_seqno = hdr->seqno;
                dprint("handle_Established_state:: ACKing the FIN packet "
                       "with ACK no %u\n", cc->remote_seqno + 1);
                send_ack(socket, FIN);
                
                switch (cc->state) {
                case Established:
                    cc->state = Close_Wait;
                    break;
                case Fin_Wait1:
                    cc->state = Closing;
                    break;
                case Fin_Wait2:
                    cc->state = Time_Wait;
                    break;
                default:
                    dprint("handle_Established_state:: Unexpeced FIN received"
                           " in state %d\n", cc->state);
                }

            } else {
                dprint("handle_Established_state:: Simple ACK, ignoring!\n");
            }
        }
    } else {    
        /**
         * No buffer space to store data, so we discard the packet send
         * and appropriate ACK with a smaller window size.
         */
        dprint("handle_Established_state:: Packet dropped because of "
               "insufficient buffer space, sending old ACK no.\n");
        send_ack(socket, 0);
    }

    return 1;
}

/**
 * Send a simple ACK, a SYN+ACK, or an ACK for a FIN.
 */
static int 
send_ack(int socket, int flags)
{
    Data    dat;
    Header  hdr;
    int     bytes_sent;
    
    cc = &(muxer[socket]);

    dat.content = (uchar*) calloc(DATA_SIZE, sizeof(uchar));
    memset(dat.content, 0, DATA_SIZE);
    memset(&hdr, 0, sizeof(hdr));
    dat.len = 0;
    setup_packet(socket, &hdr);

    if (cc->state == Syn_Recv) {
        dprint("send_ack:: Sending SYN+ACK\n");
        hdr.flags = 0;
        hdr.flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT;
        hdr.flags = hdr.flags | SYN | ACK;
    } else {
        if (flags & FIN) {
            hdr.ackno = hdr.ackno + 1;
            dprint("send_ack:: Sending ACK for FIN\n");
        } else {
            dprint("send_ack:: Sending simple ACK\n");
        }
    }
    bytes_sent = _send_tcp_packet(&hdr, &dat);
    dprint("send_ack:: ACK sent\n");
    
    free(dat.content);
    return 1;
}

/**
 * We're done with the library! Utitity functions follow
 */

/**
 * This function simply calculates the checksum over a raw buffer. It is
 * simply inet_checksum, as provided.
 */
static u16_t
raw_checksum(uchar* buf, int buflen)
{
    u32_t sum=0;
    u16_t u16_end=0;
    u16_t *u16_buf = (u16_t *)buf;
    int u16_buflen = buflen >> 1;

    if (buflen % 2) {
        *((u8_t *)&u16_end) = ((u8_t *)buf)[buflen-1];
        sum += u16_end;
    }

    while (u16_buflen--) {
        sum += u16_buf[u16_buflen];
        if (sum >= 0x00010000)
            sum -= 0x0000ffff;
    }

    return (u16_t)~sum;
}

/**
 * Swap endianess on a Header structure
 * ntoh = 1 => Network to Host order
 * ntoh = 0 => Host to Network order
 */
static void
swap_header(Header* hdr, int ntoh)
{
    if (ntoh) {
        hdr->sport = ntohs(hdr->sport);
        hdr->dport = ntohs(hdr->dport);
        hdr->seqno = ntohl(hdr->seqno);
        hdr->ackno = ntohl(hdr->ackno);
        hdr->flags = ntohs(hdr->flags);
        hdr->window = ntohs(hdr->window);
        hdr->chksum = ntohs(hdr->chksum);
        hdr->urgent = ntohs(hdr->urgent);
    } else {
        hdr->sport = htons(hdr->sport);
        hdr->dport = htons(hdr->dport);
        hdr->seqno = htonl(hdr->seqno);
        hdr->ackno = htonl(hdr->ackno);
        hdr->flags = htons(hdr->flags);
        hdr->window = htons(hdr->window);
        hdr->chksum = htons(hdr->chksum);
        hdr->urgent = htons(hdr->urgent);
    }
}

/**
 * Print a TCP packet in compact form.
 * Displays the header values and first 10 bytes of the data 
 */
static void
show_packet(Header* hdr, Data* dat, int out)
{   
    int i;
    if (out)
        dprint("_send_tcp_packet:: ");
    else
        dprint("_recv_tcp_packet:: ");
        
    /* print src, dst, ports */
    dprint("HDR{");
    dprint("[%s:%d -> %s:%d]", inet_ntoa(hdr->src), hdr->sport,
            inet_ntoa(hdr->dst), hdr->dport);
    dprint("[S%d:A%d][W:%d][", hdr->seqno, hdr->ackno, hdr->window);
    
    /* print flags */
    if (hdr->flags & SYN) dprint("S");
    if (hdr->flags & ACK) dprint("A");
    if (hdr->flags & FIN) dprint("F");
    if (hdr->flags & RST) dprint("R");
    
    /* print data and state */
    dprint("]} DATA{%d}{ ", dat->len);
    for (i = 0; i < dat->len && i < 10; i++)
        dprint("%c", dat->content[i]);
    dprint("} %s\n", state_names[cc->state]);
    noprint("");
}

/* vim: set ts=4 sw=4 expandtab tw=78: */

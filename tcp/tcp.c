#include "tcp.h"

/* util functions */
static u16_t raw_checksum(uchar* dat, int len);
static void swap_header(Header* hdr, int ntoh);
static void show_packet(Header* hdr, Data* dat, int out);

/* more support functions*/
static int can_read(int state);
static int can_write(int state);
static int handle_packets(void);
static int send_ack(int flags) ;
static int setup_packet(Header* hdr );
static int wait_for_ack(u32_t local_seqno);
static int write_packet(char* buf, int len, int flags );
static int socket_close(void) ;

/* state handling functions*/
static int handle_Listen_state(Header* hdr, Data* dat);
static int handle_Syn_Sent_state(Header* hdr, Data* dat);
static int handle_Syn_Recv_state(Header* hdr, Data* dat);
static int handle_Established_state(Header* hdr, Data* dat);

/* signal handling function */
static void alarm_signal_handler(int sig);
static int previous_alarm_time = 0 ;
static int clear_retransmission_mechanism ( void );
static void reset_retransmission_buffer (int set, Header *hdr, Data *dat );
static void (*ptr_original_signal_handler)(int sig) = SIG_IGN ;
static void restore_app_alarm ();

/* noprint() */
static int noprint(const char* format, ...) { return 1; }

/* global state (single connection for now) */
static TCPMux* Head;
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

/* static allocation for retransmission buffer */
static int      rt_present = 0;
static int      rt_counter = 0;
static Data     rt_data;
static uchar    rt_buf[DATA_SIZE];
static Header   rt_hdr;

static int      tcp_packet_counter = 0;
static int      tcp_flow_type = 0;

/* Low level send and receive functions */
static int
_send_tcp_packet(Header* hdr, Data* dat)
{
	uchar* tmp;
	int len, csum;
	/* variables for packet dropping code */
 /*	
	TCPCtl* cc;
	cc = Head->this;
*/
	if (!hdr->dst) {
		dprint("_send_tcp_packet:: destination not specified, aborting\n");
		return 0;
	}
	++tcp_packet_counter;
	hdr->src = my_ipaddr;
	hdr->prot = htons(IP_PROTO_TCP);
	hdr->tlen = htons(HEADER_SIZE + dat->len);

	show_packet(hdr, dat, 1);
	/* packet dropping code */

/*	if (cc->type == TCP_CONNECT )
	{
		if ( tcp_packet_counter == 2 )
		{
			dprint("###  _send_tcp_packet:: dropping above packet\n");
			return dat->len;
		}
	}
*/

	swap_header(hdr, 0);

	tmp = (uchar*) calloc(sizeof(Header) + dat->len, sizeof(uchar));
	memcpy(tmp, (uchar*) hdr, sizeof(Header));
	memcpy(tmp + sizeof(Header), dat->content, dat->len);
	csum = raw_checksum(tmp, sizeof(Header) + dat->len);

	memcpy(tmp + CHECK_OFF, (uchar*) & csum, sizeof(u16_t));

	len = ip_send(hdr->dst, IP_PROTO_TCP, 2,
			  (void*) (tmp + HEADER_OFF), HEADER_SIZE + dat->len);
	free(tmp);

	return dat->len;
}

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

	/* Check the checksum! */
	tmp = (uchar*) calloc(HEADER_OFF + len, sizeof(uchar));
	memcpy(tmp, (uchar*) hdr, HEADER_OFF);
	memcpy(tmp + HEADER_OFF, (uchar*) data, len);
	if (raw_checksum(tmp, HEADER_OFF + len) != 0) {
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

	/*
	 * We ignore options, just interested in data offset (which is
	 * described in number of WORD_SIZE bytes).
	 */
	doff = (hdr->flags >> DATA_SHIFT) * WORD_SIZE;
	dat->len = len - doff;
	dat->content = (uchar*) calloc(dat->len, sizeof(uchar));
	
	/*
	 * This memory is to be freed by the calling function.
	 * In this case, that's handle_packets.
	 */
	memcpy(dat->content, (uchar*) data + doff, dat->len);
	show_packet(hdr, dat, 0);

	return dat->len;
}

/* Low level interface wrapper */
int
send_tcp_packet(ipaddr_t dst, u16_t src_port, u16_t dst_port,
		u32_t seq_nb, u32_t ack_nb, u8_t flags, u16_t win_sz,
		const char* data, int data_sz)
{
	int ret;

	Header* hdr = (Header*) calloc(1, sizeof(Header));
	Data* dat = (Data*) calloc(1, sizeof(Data));
	dat->len = data_sz;
	dat->content = (uchar*) data;

	hdr->dst = dst;
	hdr->sport = src_port;
	hdr->dport = dst_port;
	hdr->seqno = seq_nb;
	hdr->ackno = ack_nb;
	hdr->flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT;
	hdr->flags = hdr->flags | flags;
	hdr->window = win_sz;

	ret = _send_tcp_packet(hdr, dat);
	free(hdr);
	free(dat);

	return ret;
}

int
recv_tcp_packet(ipaddr_t* src, u16_t* src_port, u16_t* dst_port,
		u32_t* seq_nb, u32_t* ack_nb, u8_t* flags, u16_t* win_sz,
		char* data, int* data_sz)
{
	int ret;

	Header* hdr = (Header*) calloc(1, sizeof(Header));
	Data* dat = (Data*) calloc(1, sizeof(Data));

	ret = _recv_tcp_packet(hdr, dat);

	*src = hdr->src;
	*src_port = hdr->sport;
	*dst_port = hdr->dport;
	*seq_nb = hdr->seqno;
	*ack_nb = hdr->ackno;
	*flags = (u8_t) (hdr->flags & 0xFF);
	*win_sz = hdr->window;

	data = (char*) dat->content;
	*data_sz = dat->len;

	return ret;
}

/* TCP Interface functions */

int
tcp_socket(void)
{
	TCPCtl* cc;
	TCPCtl* ctl;
	
	if (!ip_init()) {
		/* return 0; */
	}
	
	Head = (TCPMux*) calloc(1, sizeof(TCPMux));
	ctl = (TCPCtl*) calloc(1, sizeof(TCPCtl));

	/* All memory allocated here will be freed in socket_close. */

	/* Allocating memory for incoming buffer */
	ctl->in_buffer = (Data*) calloc(1, sizeof(Data));
	ctl->in_buffer->content = (uchar*) calloc(DATA_SIZE + 1, sizeof(uchar));
	memset(ctl->in_buffer->content, 0, (DATA_SIZE) * sizeof(uchar));
	ctl->in_buffer->len = 0;

	/* Allocating memory for outgoing buffer */
	ctl->out_buffer = (Data*) calloc(1, sizeof(Data));
	ctl->out_buffer->content = (uchar*) calloc(DATA_SIZE + 1, sizeof(uchar));
	memset(ctl->out_buffer->content, 0, (DATA_SIZE) * sizeof(uchar));
	ctl->out_buffer->len = 0;

	/* Clear the data buffer */
	ctl->remaining = 0;

	ctl->state = Closed;
	Head->socket = 1;
	Head->sport = 9090;
	Head->this = ctl;
	Head->next = NULL;

	cc = Head->this;

    /* FIXME: better way to set initial seq-no */
	cc->local_seqno = 100;
	/* Have to be set in three way handshake */
	cc->remote_ackno = 0;
	cc->remote_seqno = 0;
	cc->remote_window = DATA_SIZE;

	/*
	 * Now set the signal handler, for SIGALARM, which will handle the
	 * retransmissions
	 */
	clear_retransmission_mechanism ();
	
	/* FIXME : checkup for failure of tcp_socket */
	return 1;
}

static
int clear_retransmission_mechanism ( void )
{
	reset_retransmission_buffer (0,NULL, NULL);	
	return 0 ;
}

static
void reset_retransmission_buffer (int set, Header *hdr, Data *dat )
{
		/* initializing the retransmission buffer */
		rt_present = 0;
		memset(&rt_hdr, 0, sizeof(Header));
		memset(&rt_data, 0, sizeof(Data));
		memset(&rt_buf, 0, DATA_SIZE);
		rt_data.content = rt_buf;
		rt_data.len = 0;
		rt_counter = 0;

	if (set == 1 )
	{

		memcpy (&rt_hdr, hdr, sizeof (Header));
		memcpy(rt_data.content, dat->content, dat->len);
		rt_data.len = dat->len;
		rt_present = 1;
		rt_counter = 0;
		ptr_original_signal_handler = signal(SIGALRM, alarm_signal_handler) ;
	   
		if ( ptr_original_signal_handler == SIG_ERR) {
			dprint("tcp_socket:: Can't set signal handler!\n");
			exit(1) ;
		}
	}
}

/* Connection oriented part */

int
tcp_connect(ipaddr_t dst, int port)
{
	TCPCtl* cc;
	char buffer = 0;

	cc = Head->this;

	/*
	 * FIXME: make sure that tcp_socket is called before calling this
	 * function
	 */

	/* setting up status variables for this connection */
	cc->type = TCP_CONNECT;
	tcp_flow_type = TCP_CONNECT;
	
	/* FIXME: Sequence number allocation */
	cc->local_seqno += 100;
	Head->dport = port;
	Head->dst = dst;
	
	/* FIXME: better way to assign local port */
	Head->sport = 6000;
	cc->state = Syn_Sent;

	/* Send SYN packet */
	write_packet(&buffer, 0, SYN);

    dprint("tcp_connect:: Done, calling handle_packets\n");
	while (cc->state != Established) {
		handle_packets();
		if (cc->state == Closed )
		{
			return -1 ;
		}
	}
	return 1;
}


int
tcp_listen(int port, ipaddr_t* src)
{
	TCPCtl* cc;
	cc = Head->this;

	/*
	 * FIXME: make sure that tcp_socket is called before calling this
	 * function
	 */

	/* now change the state */
	cc->type = TCP_LISTEN;
	tcp_flow_type = TCP_LISTEN;
	cc->state = Listen;
	
	/* FIXME: better way to set initial seq-no */
	cc->local_seqno += 400;
	Head->sport = port;

    dprint("tcp_listen:: Done, calling handle_packets\n");
	do {
		handle_packets();
		if (cc->state == Closed)
		{
			dprint("tcp_listen:: prob occured, other side is not responding\n");
			return -1 ;
			
		}
	} while (cc->state != Established);

	return 1;
}


int
tcp_read(char* buf, int maxlen)
{
    TCPCtl* cc;
	int     remaining_bytes = 0;
	int     bytes_reading = 0;
	int     can_read_max = 0;
	int     total_bytes_read = 0;
	int     old_window, new_window;

	cc = Head->this;

	/* sanity checks */
	if (buf == NULL || maxlen < 0)
		return -1;

	/* check if the connection from other side is closed... */
	/* if yes, then return EOF */
	if (can_read(cc->state) == Closed) {
		dprint("tcp_read:: Error! State is %d, cannot read data\n",
                   cc->state);
		return EOF;
	}

	if (maxlen == 0)
		return 0;

	bytes_reading = total_bytes_read = 0 ;
	can_read_max = maxlen ;
	while ( total_bytes_read <  maxlen ) {

		can_read_max = maxlen - total_bytes_read ;

	while (cc->in_buffer->len == 0) {
		/* check if the connection from other side is closed... */
		/* if yes, then return EOF */
		if (can_read(cc->state) == 0) {
			dprint("tcp_read:: Error! State is %d, cannot read data\n",
                    cc->state);
			if (total_bytes_read == 0 )
			{
				return EOF;
			}
			else
			{
				return total_bytes_read ;
			}
		}
		dprint("tcp_read:: No data in read buffer, calling handle packets\n");
		handle_packets();
	}

	old_window = DATA_SIZE - cc->in_buffer->len;
	bytes_reading = MIN(cc->in_buffer->len, can_read_max );
	
	/* copy the bytes into buf given by user */
	memcpy(buf + total_bytes_read , cc->in_buffer->content, bytes_reading);
	remaining_bytes = cc->in_buffer->len - bytes_reading;

	if (remaining_bytes > 0) {
		memmove(cc->in_buffer->content,
				(cc->in_buffer->content + bytes_reading), remaining_bytes);
	}
	cc->in_buffer->len = remaining_bytes;
	total_bytes_read += bytes_reading ;
	
	/* clear the data buffer */
	memset(cc->in_buffer->content + remaining_bytes, 0,
			(DATA_SIZE - remaining_bytes) * sizeof(uchar));

	new_window = DATA_SIZE - cc->in_buffer->len;
	if (old_window == 0) {
		if (new_window != 0) {
			/* created free space in buffer */
			/* tell the sender about it by sending ack packet */
			dprint("tcp_read:: Sending ack for advertising free buffer %d\n",
                    new_window);
			send_ack(0);
		}
	}
	
	/*
		 * this function supports both mode if following return
		 * statement is commented, then tcp_write will assure thate
		 * all data given to it is sent if following return statement
		 * is not commented, then tcp write will send only that much
		 * data which can fit into one packet, and then return the
		 * bytes_sent to calling function
		 */

	dprint ("tcp_read : read %d bytes successfully\n", total_bytes_read);
	return total_bytes_read; 

	} /* end while : total_bytes_read < maxbuf */
	return total_bytes_read;
}


int
tcp_write(char* buf, int len)
{
    TCPCtl* cc;
	int     bytes_left;
	int     bytes_sent;
	int     packet_size;
	int     temp;
	
	cc = Head->this;

	
	bytes_sent = 0;
	bytes_left = len;
	dprint("tcp_write:: Writing %d bytes\n", len);
	
	while (bytes_left > 0) {
		/* check if the connection from your side is closed... */
		/* if yes, then return error */
		if (can_write(cc->state) == Closed) {
			dprint("tcp_write:: Error! State is %d, cannot write data\n",
                cc->state);
			if (bytes_sent == 0 )
				return -1;
			else return bytes_sent ;
		}
		/*
		 * if (cc->remote_window < bytes_left ) packet_size =
		 * cc->remote_window ; else packet_size = bytes_left ;
		 */
		packet_size = MIN(cc->remote_window, bytes_left);
		while (packet_size == 0) {
			dprint("tcp_write:: Remote window is empty %d(%d,%d), waiting\n",
                    packet_size, cc->remote_window, bytes_left);
			handle_packets();
			packet_size = MIN(cc->remote_window, bytes_left);
		}


		dprint("tcp_write:: Writing %d bytes with write_packet\n",
                packet_size);
		temp = write_packet(buf + bytes_sent, packet_size, 0);
		if (temp ==  -1 )
		{
			if (cc->state == Closed )
			{
			if (bytes_sent == 0)
			{
				dprint("tcp_write:: other side is closed\n");
				return -1 ;
			}
			else
			{
				dprint("tcp_write:: other side is closed\n");
				return bytes_sent ;	
			}
			}
		}
		bytes_sent += temp;

		bytes_left = len - bytes_sent;

		/*
		 * this function supports both mode if following return
		 * statement is commented, then tcp_write will assure thate
		 * all data given to it is sent if following return statement
		 * is not commented, then tcp write will send only that much
		 * data which can fit into one packet, and then return the
		 * bytes_sent to calling function
		 */
/*		return bytes_sent; */ 
	}
		
    return bytes_sent;
}


int
tcp_close(void)
{
	TCPCtl* cc;
	int     ret;
	char    buffer = 0;
	cc = Head->this;


	/* send FIN and start 4-way closing procedure */
	/* clear variables */
	dprint("tcp_close:: Called... ");
	if (cc->state == Established || cc->state == Close_Wait )
	{
		/* send FIN packet */
		ret = write_packet(&buffer, 0, FIN);
	}

	/*
	 * it means got the ACK for your FIN, so, lets change the state
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
		socket_close ();
		break;

	case Closed:
		dprint("Already in Closed state\n");
		cc->state = Closed;
		socket_close ();
		break;

	default:
		dprint("Error: Odd state %d %s\n", cc->state, state_names[cc->state]);
		break;

	}

	return 1;
}


/* signal handler for SIG_ALARM, which will deal with retransmissions */
static void
alarm_signal_handler(int sig)
{
	TCPCtl* cc;
	Header  c_hdr;
	Data    c_data;
	int     bytes_sent;
	
	cc = Head->this;
	
	/* set the signal handler again */
	if (signal(SIGALRM, alarm_signal_handler) == SIG_ERR) {
		dprint("alarm_signal_handler:: Can't set signal handler!\n");
		return;
	}
	if (!rt_present) {
		/* no unack packet for retransmission */
		return;
	}
	/* there is packet for restransmission. */
	++(rt_counter);

	/* re-send the packet */

	c_hdr = rt_hdr;
	c_data = rt_data;
	
	/*
	 * update the ack_no and window_size as it may change
	 */
	dprint("alarm_signal_handler:: Re-transmitting packet: %d\n", rt_counter);
	rt_hdr.ackno = cc->remote_seqno;
	rt_hdr.window = DATA_SIZE - cc->in_buffer->len;
	bytes_sent = _send_tcp_packet(&c_hdr, &c_data);
	
	/* FIXME: check for return value of send_tcp_packet */
	/* set alarm, in case even this packet is lost */
	alarm(RETRANSMISSION_TIMER);
	/* cancel the signal SIGALARM */
	previous_alarm_time -= RETRANSMISSION_TIMER;
}


/* Private functions */

/* Checks if you can read from socket or not */
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

/* Checks if you can write into socket or not */
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
 * create a tcp packet out of given data and send it also, handle
 * retransmission till you get correct ACK
 */
static int
write_packet(char* buf, int len, int flags)
{
	TCPCtl* cc;
	Header  hdr;
	Data    dat;
	int     bytes_sent;
	/* what ack_no to wait for? */
	u32_t   ack_to_wait; 

	cc = Head->this;

	/* clear variables */
	memset(&hdr, 0, sizeof(hdr));
	memset(&dat, 0, sizeof(dat));
	
	/* assign data */
	dat.content = (uchar *) buf;
	dat.len = len;

	/* setup packet */
	setup_packet(&hdr);
	/* updating local sequence number */
	cc->local_seqno = cc->local_seqno + len;
	ack_to_wait = cc->local_seqno;

	/* handle syn packets */
	if (flags & SYN) {
		/* resetting the flags to be the first SYN packet */
		hdr.flags = 0;	/* clearing flags first */
		hdr.flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT;
		hdr.flags = hdr.flags | SYN;	/* setup default flags */
		/* SYN is single bit data, so incrementing ack to wait by one */
		ack_to_wait = cc->local_seqno + 1;
	}
	if (flags & FIN) {
		/* resetting the flags to be the FIN packet */
		hdr.flags = hdr.flags | FIN; /* setup default flags */

		/* FIN is single bit data, incrementing local_seqno by one */
		if (len == 0) {
			/*
			 * after sending FIN packet, should i increment seq
			 * no by one ?? the RFC example says yes, but test
			 * written in CNP does not seems to agree with it.
			 */
			/* cc->local_seqno = cc->local_seqno + 1 ; */
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
	reset_retransmission_buffer (1,&hdr, &dat) ;
/*	rt_hdr = hdr;
	memcpy(rt_data.content, dat.content, dat.len);
	rt_data.len = dat.len;
	rt_present = 1;
*/

	/* send the packet */
	/* in case of retransmission, update the ack_no and window_size */
	hdr.ackno = cc->remote_seqno;
	hdr.window = DATA_SIZE - cc->in_buffer->len;
	bytes_sent = _send_tcp_packet(&hdr, &dat);

	if ( wait_for_ack(ack_to_wait) == -1 )
	{
		dprint("write_packet:: error : sending packet failed, no ack received!\n");
		return -1 ;
	}
	dprint("write_packet:: Packet sent successfully (ACK received)!\n");
	return bytes_sent;
}

static int
is_valid_ack(u32_t local_seqno, u32_t remote_ackno)
{
    TCPCtl* cc;
	int     diff;
	
	cc = Head->this;

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
		/* Wrap around */
		return 1;
	}

    dprint("is_valid_ack:: Invalid ACK!\n");
	return 0;
}

static int
wait_for_ack(u32_t local_seqno)
{
	TCPCtl* cc;
	cc = Head->this;

	previous_alarm_time = alarm(RETRANSMISSION_TIMER);

	while (!is_valid_ack(local_seqno, cc->remote_ackno)) {
		dprint("wait_for_ack:: Calling handle_packets\n");
		handle_packets();
		if (cc->state == Last_Ack) {
			if (rt_counter >= 2) {
				cc->state = Closed;
				restore_app_alarm ();
				return -1;
			}
		}
		if (cc->state == Closed) {
			restore_app_alarm ();
			return -1;
		}
		if (cc->state == Closing) {
			if (rt_counter >= 4) {
				cc->state = Closed;
				restore_app_alarm ();
				return -1;
			}
		}
		if (rt_counter >= 4) {
			cc->state = Closed;
			restore_app_alarm ();
			return -1;
		}
		dprint("wait_for_ack:: Got ack %u, but expected ack %u\n",
                cc->remote_ackno, local_seqno);
	}

	/*
	 * FIXME: few assumptions Whenever we will get correct ACK, we will
	 * assume that last packet which is present in list of unack packet
	 * is acked, so delete it. and cancel the alarm which was set.
	 */


	dprint("wait_for_ack:: Got good ack %u / %u, canceling alarm\n",
            cc->remote_ackno, local_seqno);

		restore_app_alarm ();
	return 1;
}


static void restore_app_alarm ()
{

	int time_missed;
	reset_retransmission_buffer (0,NULL, NULL );

	if (signal(SIGALRM, ptr_original_signal_handler) == SIG_ERR) 
	{
		dprint("tcp_socket:: Can't set signal handler!\n");
		exit(1) ;
	}

	/* cancel the signal SIGALARM */
	time_missed = alarm(0);
/*	previous_alarm_time -= time_missed ;*/
	alarm (previous_alarm_time ) ;

}


static int 
setup_packet(Header* hdr)
{
	TCPCtl* cc;
	
	cc = Head->this;

	hdr->dst = Head->dst;
	hdr->sport = Head->sport;
	hdr->dport = Head->dport;
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

/* A function which will release all resources alloted to socket */
static int
socket_close(void)
{
	TCPCtl* cc;

	cc = Head->this;
	cc->state = Closed;
	
	free(cc->in_buffer->content);
	free(cc->out_buffer->content);
	free(cc->in_buffer);
	free(cc->out_buffer);


	return 1;
}

/*
 * waits for one incomming packet put the data in incoming buffer and update
 * the ack, remote seq no.
 */
static int
handle_packets()
{
    TCPCtl*     cc;
	Header      hdr;
	Data        dat;
	int         len, ret;

	cc = Head->this;

	if (cc->state ==  Closed )
	{
		return -1 ;
	}
	dprint("handle_packets:: State %s, waiting for packet\n",
            state_names[cc->state]);

	do {
		len = _recv_tcp_packet(&hdr, &dat);
		dprint("handle_packets:: Received packet with len %d\n", len);
		if (cc->state == Last_Ack) {
			if (rt_counter >= 2) {
				cc->state = Closed;
				return -1;
			}
		}
			if (rt_counter >= 5) {
				cc->state = Closed;
				return -1;
			}
		
	} while (len < 0);

	if (hdr.dport != Head->sport) {
		dprint("handle_packets:: Received packet for wrong port %u, "
			   "expecting %u\n", hdr.dport, Head->sport);
		return -1;
	}
	
	dprint("handle_packets:: Dealing with %s\n", state_names[cc->state]);
	
	switch (cc->state) {

	case Closed:
        ret = 1;
		break;

	case Listen:
		ret = handle_Listen_state(&hdr, &dat);
		break;

	case Syn_Sent:
		ret = handle_Syn_Sent_state(&hdr, &dat);
		break;

	case Syn_Recv:
		ret = handle_Syn_Recv_state(&hdr, &dat);
		break;

	case Established:
	case Fin_Wait1:
	case Fin_Wait2:
	case Close_Wait:
	case Closing:
	case Last_Ack:
		ret = handle_Established_state(&hdr, &dat);
		break;

	default:
		dprint("handle_packets:: Error! Unplanned state %d\n", cc->state);
		break;

	}

	/* freeing up the memory alloted by recv_tcp_packet() */
	free(dat.content);
	return 1;
}

/*
 * handle_Listen_state : should expect syn packet should send syn+ack packet
 */
static int 
handle_Listen_state(Header* hdr, Data* dat)
{
	TCPCtl* cc;
	int     flags;

	cc = Head->this;
	flags = hdr->flags;

	if (!(flags & SYN)) {
		dprint("handle_Listen_state:: No SYN set, ignoring!\n");
		return -1;
	}
	
	/* Ok, it's a SYN packet */
	/* Get information about other side */
	Head->dst = hdr->src;
	Head->dport = hdr->sport;
	cc->remote_seqno = hdr->seqno + 1;
	/* updating remote window size */
	cc->remote_window = hdr->window;	
	cc->state = Syn_Recv;
	
	dprint("handle_Listen_state:: Got SYN, sending SYN+ACK\n");
	send_ack(SYN);

	return 1;
}

/*
 * handle_Syn_Sent_state : will be called from connect should expect syn+ack
 * packet and should send ack packet
 */
static int 
handle_Syn_Sent_state(Header* hdr, Data* dat)
{
    TCPCtl* cc;
	int     flags;
	
	cc = Head->this;
	flags = hdr->flags;

	if (!(flags & SYN)) {
		dprint("handle_Syn_Sent_state: No SYN set, ignoring!\n");
		return -1;
	}
	
	/*
	 * Ok, it's a SYN packet. Check if there's an ack with it.
	 */
	if (flags & ACK) {
		/* check if ackno received is correct or not */
		if (cc->local_seqno + 1 != hdr->ackno) {
			/* Got wrong ack-no... ignoring */
			dprint("handle_Syn_Sent_state:: SYN+ACK were set, but with "
				   "the wrong sequence no., ignoring!\n");
			return -1;
		}
		
		dprint("handle_Syn_Sent_state:: Got correct SYN+ACK packet, "
               "sending ACK\n");

		/* Got correct ack no... updating variables */
		cc->remote_ackno = hdr->ackno;
		cc->local_seqno = hdr->ackno;
		cc->remote_seqno = hdr->seqno + 1;
		cc->remote_window = hdr->window;
		
		/* need to send ack for this */
		cc->state = Established;
		send_ack(0);
		return 1;
	}

	/*
	 * Got just SYN packet. It's a case of simultaneous open
	 */
	dprint("handle_Syn_Sent_state:: Got just SYN, simultaneous open. "
		   "Sending SYN+ACK\n");

	cc->state = Syn_Recv;
	cc->remote_seqno = hdr->seqno + 1;
	cc->remote_window = hdr->window;
	
	send_ack(0);
	return 1;
}

static int 
handle_Syn_Recv_state(Header* hdr, Data* dat)
{
    TCPCtl* cc;
	int     flags;
	
	cc = Head->this;
	flags = hdr->flags;

	if (!(flags & ACK)) {
		dprint("handle_Syn_Recv_state:: ACK not set, ignoring!\n");
		/*
		 * checking if it's retransmisson SYN packet, because SYN+ACK
		 * was dropped
		 */
		return (handle_Listen_state(hdr, dat));
	}
	
	/*
	 * FIXME: you may get FIN packet at this stage... then you need to
	 * change to FIN_WAIT_1 state
	 */

	/* Got ACK, check if it is the correct ACK */
	if (cc->local_seqno + 1 != hdr->ackno) {
		/* Got wrong ack-no... ignoring the packet */
		dprint("handle_Syn_Recv_state:: ACK set, but with wrong ACK no., "
			   "ignoring!\n");
		return -1;
	}
	
	/* Ok, got ACK for my SYN, moving to Established state */
	cc->remote_ackno = hdr->ackno;
	cc->local_seqno = hdr->ackno;
	cc->remote_seqno = hdr->seqno + dat->len;
	cc->remote_window = hdr->window;
	
	/* need to send ack for this */
	cc->state = Established;
	dprint("handle_Syn_Recv_state:: Got ACK, going to Established state\n");
	
	/* check if we have enough space in incoming buffer */
	if (dat->len > 0) {
		dprint("handle_Syn_Recv_state:: ACK packet contains data!\n");
		if (cc->in_buffer->len + dat->len <= DATA_SIZE) {
			/*
			 * there is data in this packet, and also there is
			 * space in buffer so, put this data in buffer
			 */
			memcpy(cc->in_buffer->content+cc->in_buffer->len, dat->content, dat->len);
			cc->in_buffer->len += dat->len;

			cc->remote_seqno = hdr->seqno + dat->len;
		}
	}
	return 1;
}

/*
 * handle_Established_state : you can receive following packets 1. data
 * packets 2. ack packets 3. fin packets
 */
static int 
handle_Established_state(Header * hdr, Data * dat)
{
	TCPCtl* cc;
	cc = Head->this;

	/*
	 * FIXME: in case of Closing state may be hdr->seqno may b
	 * incremented by one... as this behaviour is not clear in RFC
	 */
	/* check if it is correct packet */
	if (cc->remote_seqno != hdr->seqno) {
		dprint("handle_Established_state:: Received incorrect packet, "
			   "expected %u, received %u, ACKing with old ACK number\n",
                cc->remote_seqno, hdr->seqno);
		/*
		 * sending ack, just to tell other side dat something is
		 * wrong.
		 */
		send_ack(0);
		return -1;
	}
	/* update the ack number received */
	cc->remote_ackno = hdr->ackno;
	cc->remote_window = hdr->window;

	/* check if we have enough space in incoming buffer */
	if ((dat->len == 0) || (cc->in_buffer->len + dat->len <= DATA_SIZE)) {
		/* accept the packet */
		dprint("handle_Established_state:: Receive %d bytes\n", dat->len);
		memcpy(cc->in_buffer->content + cc->in_buffer->len, dat->content, dat->len);
		cc->in_buffer->len += dat->len;

		cc->remote_seqno = hdr->seqno + dat->len;

		/*
		 * if packet contain data or if it's fin packet, then only
		 * acknowledge it
		 */
		if (dat->len != 0) {
			if (hdr->flags & FIN) {
                dprint("handle_Established_state:: ACKing the FIN packet "
                       "(packet had data %d) with ACK no %u\n",
                       dat->len, cc->remote_seqno);
			}
			send_ack(0); /* normal send ack*/
			dprint ("handle_Established_state: send simple ack \n");
		} else {
			if (hdr->flags & FIN) {
				/* as FIN flag is 1 bit of data */
				/* cc->remote_seqno = hdr->seqno + 1 ; */
				cc->remote_seqno = hdr->seqno;
				dprint("handle_Established_state:: ACKing the FIN packet "
					   "with ACK no %u\n", cc->remote_seqno + 1);
				send_ack(FIN);
				
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
		/* no buffer to store data, discard the
		 * packet send the ack with old ack number
		 * and with smaller window size */
        dprint("handle_Established_state:: Packet dropped because of "
               "insufficient buffer space, sending old ACK no.\n");
		send_ack(0);
	}

	return 1;
}

/* Support functions */
static int 
send_ack(int flags)
{
    TCPCtl* cc;
	Data    dat;
	Header  hdr;
	int     bytes_sent;
	
	cc = Head->this;

	dat.content = (uchar*) calloc(DATA_SIZE, sizeof(uchar));
	memset(dat.content, 0, DATA_SIZE);
	memset(&hdr, 0, sizeof(hdr));
	dat.len = 0;
	setup_packet(&hdr);

	if (cc->state == Syn_Recv) {
		/* need to send SYN+ACK */
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

/* Utility functions */

/* This function simply calculates the checksum over a raw buffer */
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

/* Swaps endianess on a Header structure */
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

/* Print a TCP packet in compact form */
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
	for (i = 0; i < dat->len; i++)
		dprint("%c", dat->content[i]);
	dprint("} %s\n", state_names[Head->this->state]);
	noprint("");
}

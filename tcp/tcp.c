#include "tcp.h"

TCPMux* Head;

char state_names[][12] = {
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
int             rt_present = 0;
Header          rt_hdr;
Data            rt_data;
uchar           rt_buf[DATA_SIZE];
int             rt_counter = 0;

int             tcp_packet_counter = 0;
int             tcp_flow_type = 0;
int             DROP_FLOW_DIRECTION = -1;
int             DROP_PACKET_COUNTER = -1;

/* Low level interface wrapper */
int
send_tcp_packet(ipaddr_t dst, u16_t src_port, u16_t dst_port,
		u32_t seq_nb, u32_t ack_nb, u8_t flags, u16_t win_sz,
		const char *data, int data_sz)
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
		char*data, int*data_sz)
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

/* Low level send and receive functions */
int
_send_tcp_packet(Header* hdr, Data* dat)
{
	uchar* tmp;
	int len, csum;

	if (!hdr->dst) {
		dprint("send_tcp_packet: Error: Destination not specified ");
		return 0;
	}
	++tcp_packet_counter;
	hdr->src = my_ipaddr;
	hdr->prot = htons(IP_PROTO_TCP);
	hdr->tlen = htons(HEADER_SIZE + dat->len);
	
	/* For detailed Debugging */
	/*
	 * dprint("Checksum calcuated (Network order): %x\n", csum);
	 * dprint("Packet sending : header and data dumped\n" );
	 * dump_header(hdr); dump_buffer(dat->content, dat->len);
	 */
	ddprint("\n### out ");
	show_packet(hdr, dat->content, dat->len);
	
	/*
	 * Code for packet dropping
	 */
	if (tcp_flow_type == DROP_FLOW_DIRECTION) {
		if (tcp_packet_counter == DROP_PACKET_COUNTER) {
			ddprint("\n### Dropping above packet...\n");
			return dat->len;
		}
	}
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

int
_recv_tcp_packet(Header* hdr, Data* dat)
{
    char*       data;
    uchar*      tmp;
	int         len, doff;
	ushort      prot, idp;
	ipaddr_t    src, dst;

	/*
	 * looping is not good idea, as it interfere with signals
	 */
	/*
	 * do { len = ip_receive(&src, &dst, &prot, &idp, &data);
	 * 
	 * } while (len < 0 );
	 */

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
		dprint("Incorrect Checksum! Discarding packet...");
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
	 * this memory is to be freed by calling function it's freed in
	 * handle_packets function
	 */
	memcpy(dat->content, (uchar*) data + doff, dat->len);
	/*
		// For detailed Debugging
		dprint ("packet received : Printing header and data\n");
		dump_header(hdr);
		dump_buffer(dat->content, dat->len);
	*/
	ddprint("\n### in ");
	show_packet(hdr, dat->content, dat->len);

	return dat->len;
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

	/* all memory alloted here will be freed in socket_close call. */

	/* allocating memory for incomming buffer */
	ctl->in_buffer = (Data*) calloc(1, sizeof(Data));
	ctl->in_buffer->content = (uchar*) calloc(DATA_SIZE + 1, sizeof(uchar));
	memset(ctl->in_buffer->content, 0, (DATA_SIZE) * sizeof(uchar));
	ctl->in_buffer->len = 0;

	/* allocating memory for outgoing buffer */
	ctl->out_buffer = (Data*) calloc(1, sizeof(Data));
	ctl->out_buffer->content = (uchar*) calloc(DATA_SIZE + 1, sizeof(uchar));
	memset(ctl->out_buffer->content, 0, (DATA_SIZE) * sizeof(uchar));
	ctl->out_buffer->len = 0;

	/* clear the data buffer */
	ctl->remaining = 0;

	ctl->state = Closed;
	Head->socket = 1;
	Head->sport = 9090;
	Head->this = ctl;
	Head->next = NULL;

	cc = Head->this;

    /* FIXME : better way to set initial seq-no */
	cc->local_seqno = 100;
	/* Have to be set in three way handshake */
	cc->remote_ackno = 0;
	cc->remote_seqno = 0;
	cc->remote_window = DATA_SIZE;

	/*
	 * now set the signal handler, for SIGALARM, which will handle the
	 * retransmissions
	 */

	/* initializing the retransmission buffer */
	rt_present = 0;
	memset(&rt_hdr, 0, sizeof(Header));
	memset(&rt_data, 0, sizeof(Data));
	memset(&rt_buf, 0, DATA_SIZE);
	rt_data.content = rt_buf;
	rt_data.len = 0;
	rt_counter = 0;
	if (signal(SIGALRM, alarm_signal_handler) == SIG_ERR) {
		dprint("Socket Error : can't set signal handler\n");
		return -1;
	}
	
	/* FIXME : checkup for failure of tcp_socket */
	return 1;
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
	/* FIXME: Seq no allocation */
	cc->local_seqno += 100;
	Head->dport = port;
	Head->dst = dst;
	/* FIXME : better way to assign local port */
	Head->sport = 6000;
	cc->state = Syn_Sent;

	/* send SYN packet */
	write_packet(&buffer, 0, SYN);

	while (cc->state != Established) {
		handle_packets();
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
	
	/* FIXME : better way to set initial seq-no */
	cc->local_seqno += 400;
	Head->sport = port;

	do {
		handle_packets();
	} while (cc->state != Established);

	return 1;
}


int
tcp_read(char* buf, int maxlen)
{
    TCPCtl* cc;
	int     remaining_bytes = 0;
	int     bytes_read = 0;
	int     old_window, new_window;

	cc = Head->this;


	/* sanity checks */
	if (buf == NULL || maxlen < 0)
		return -1;
	if (maxlen == 0)
		return 0;

	while (cc->in_buffer->len == 0) {
		/* check if the connection from other side is closed... */
		/* if yes, then return EOF */
		if (can_read(cc->state) == 0) {
			dprint("tcp_read:Error: State is %d, can-not read data\n",
                    cc->state);
			return EOF;
		}
		dprint("tcp_read: no data in read buffer, calling handle packets\n");
		handle_packets();
	}

	old_window = DATA_SIZE - cc->in_buffer->len;
	bytes_read = MIN(cc->in_buffer->len, maxlen);
	/* copy the bytes into buf given by user */
	memcpy(buf, cc->in_buffer->content, bytes_read);
	remaining_bytes = cc->in_buffer->len - bytes_read;

	if (remaining_bytes > 0) {
		memmove(cc->in_buffer->content,
		        (cc->in_buffer->content + bytes_read), remaining_bytes);
	}
	cc->in_buffer->len = remaining_bytes;
	
	/* clear the data buffer */
	memset(cc->in_buffer->content + remaining_bytes, 0,
	        (DATA_SIZE - remaining_bytes) * sizeof(uchar));

	new_window = DATA_SIZE - cc->in_buffer->len;
	if (old_window == 0) {
		if (new_window != 0) {
			/* created free space in buffer */
			/* tell the sender about it by sending ack packet */
			ddprint("tcp_read: send ack for advertising the free buffer %d\n",
                    new_window);
			send_ack(0);
		}
	}
	
	return bytes_read;
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

	/* check if the connection from your side is closed... */
	/* if yes, then return error */
	if (can_write(cc->state) == 0) {
		dprint("tcp_write: Error : State is %d, can-not write data\n",
                cc->state);
		return -1;
	}
	
	bytes_sent = 0;
	bytes_left = len;
	dprint("tcp_write: writing %d bytes\n", len);
	
	while (bytes_left > 0) {
		/*
		 * if (cc->remote_window < bytes_left ) packet_size =
		 * cc->remote_window ; else packet_size = bytes_left ;
		 */
		packet_size = MIN(cc->remote_window, bytes_left);
		while (packet_size == 0) {
			dprint("tcp_write: remote window is empty %d (%d, %d), waiting\n",
                    packet_size, cc->remote_window, bytes_left);
			handle_packets();
			packet_size = MIN(cc->remote_window, bytes_left);
		}


		dprint("tcp_write: writing %d bytes of packet with write_packet\n",
                packet_size);
		temp = write_packet(buf + bytes_sent, packet_size, 0);
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
		return bytes_sent;
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
	dprint("Starting the Close procedure\n");
	
	/* send FIN packet */
	ret = write_packet(&buffer, 0, FIN);

	/*
	 * it means got the ACK for your FIN, so, lets change the state
	 */
	switch (cc->state) {

	case Fin_Wait1:
		dprint("tcp_close : Fin Acknowledged, going to Fin_Wait2\n");
		cc->state = Fin_Wait2;
		break;

	case Closing:
		dprint("tcp_close: Closing: Fin Acknowledged, going to Time_Wait\n");
		cc->state = Time_Wait;
		break;

	case Last_Ack:
		dprint("tcp_close: Last_Ack: Fin Acknowledged, going to Closed\n");
		cc->state = Closed;
		/* FIXME : should call socket_close() here */
		break;

	case Closed:
		dprint("tcp_close: already in closed state\n");
		cc->state = Closed;
		/* FIXME : should call socket_close() here */
		break;

	default:
		dprint("tcp_close: error : odd state %d\n", cc->state);
		break;

	}

	return 1;
}


/* signal handler for SIG_ALARM, which will deal with retransmissions */
void
alarm_signal_handler(int sig)
{
	TCPCtl* cc;
	Header  c_hdr;
	Data    c_data;
	int     bytes_sent;
	
	cc = Head->this;
	
	/* set the signal handler again */
	if (signal(SIGALRM, alarm_signal_handler) == SIG_ERR) {
		ddprint("Socket Error : can't set signal handler\n");
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
	ddprint("\n### retransmitting packet rt-no %d", rt_counter);
	rt_hdr.ackno = cc->remote_seqno;
	rt_hdr.window = DATA_SIZE - cc->in_buffer->len;
	bytes_sent = _send_tcp_packet(&c_hdr, &c_data);
	/* FIXME: check for return value of send_tcp_packet */

	/* set alarm, in case even this packet is lost */
	alarm(RETRANSMISSION_TIMER);
	return;
}


/* Private functions */

/* Checks if you can read from socket or not */
int
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
int
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
int
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
			dprint("write_packet: state changed to Fin_Wait1 %d\n",
                    Fin_Wait1);
			break;

		case Close_Wait:
			cc->state = Last_Ack;
			dprint("write_packet: state changed to Last_Ack %d\n",
			        Last_Ack);
			break;
		}
	}
	
	/* Put the packet into the list of unacked packets */
	rt_hdr = hdr;
	memcpy(rt_data.content, dat.content, dat.len);
	rt_data.len = dat.len;
	rt_present = 1;

	/* send the packet */
	/* in case of retransmission, update the ack_no and window_size */
	hdr.ackno = cc->remote_seqno;
	hdr.window = DATA_SIZE - cc->in_buffer->len;
	bytes_sent = _send_tcp_packet(&hdr, &dat);

	wait_for_ack(ack_to_wait);
	dprint("write_packet: Packet sent successfully\n");
	return bytes_sent;
}


int
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
	dprint("is_valid_ack: [diff(%d) = remote_ack(%u) - local_seq(%u)],
            actual_local_seq(%u)\n", diff, remote_ackno, local_seqno,
            cc->local_seqno);

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
			dprint("is_valid_ack: SYN or FIN packet case\n");
			return 0;
		}
		/* Wrap around */
		return 1;
	}

    dprint("is_valid_ack: invalid ack \n");
	return 0;
}

int
wait_for_ack(u32_t local_seqno)
{
	TCPCtl* cc;
	cc = Head->this;

	alarm(RETRANSMISSION_TIMER);

	while (!is_valid_ack(local_seqno, cc->remote_ackno)) {
		dprint("wait_for_ack: calling handle_packets\n");
		handle_packets();
		if (cc->state == Last_Ack) {
			if (rt_counter == 2) {
				cc->state = Closed;
				return -1;
			}
		}
		if (cc->state == Closed) {
			return -1;
		}
		if (cc->state == Closing) {
			if (rt_counter == 4) {
				cc->state = Closed;
				return -1;
			}
		}
		dprint("wait_for_ack: got ack %u, but expected ack %u\n",
                cc->remote_ackno, local_seqno);
	}

	/*
	 * FIXME: few assumptions Whenever we will get correct ACK, we will
	 * assume that last packet which is present in list of unack packet
	 * is acked, so delete it. and cancel the alarm which was set.
	 */
	rt_present = 0;
	memset(&rt_hdr, 0, sizeof(Header));
	memset(&rt_buf, 0, DATA_SIZE);
	rt_data.content = rt_buf;
	rt_data.len = 0;
	rt_counter = 0;

	/* cancel the signal SIGALARM */
	alarm(0);
	dprint("wait_for_ack: got gud ack %u, for expected ack %u, canceling
            alarm\n", cc->remote_ackno, local_seqno);

	return 1;
}

int 
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
	
	/*
	 * dprint ( "window size = %d - %d", DATA_SIZE, cc->in_buffer->len);
	 * dprint ( " = %d\n", hdr->window );
	 */
	hdr->urgent = 1;
	hdr->flags = 0;
	hdr->flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT;
	hdr->flags = hdr->flags | URG | ACK | PSH;

	return 1;
}

/* A function which will release all resources alloted to socket */
int
socket_close(void)
{
	TCPCtl* cc;
	
	cc = Head->this;
	cc->state = Closed;
	
	free(cc->in_buffer->content);
	free(cc->out_buffer->content);
	free(cc->in_buffer);
	free(cc->out_buffer);

	/* clear all alarm signals */
	/*
	 * now set the signal handler, for SIGALARM, which will handle the
	 * retransmissions
	 */
	if (signal(SIGALRM, SIG_DFL) == SIG_ERR) {
		dprint("Socket close Error : can't remove signal handler\n");
		return -1;
	}
	alarm(0);

	return 1;
}

/*
 * waits for one incomming packet put the data in incoming buffer and update
 * the ack, remote seq no.
 */
int
handle_packets()
{
    TCPCtl*     cc;
	Header      hdr;
	Data        dat;
	int         len, ret;

	cc = Head->this;

	dprint("handle_packets: state = %s, waiting for packet\n",
            state_names[cc->state]);

	do {
		len = _recv_tcp_packet(&hdr, &dat);
		dprint("handle_packets: received packet with len %d\n", len);
		if (cc->state == Last_Ack) {
			if (rt_counter == 2) {
				cc->state = Closed;
				return -1;
			}
		}
	} while (len < 0);

	if (hdr.dport != Head->sport) {
		dprint("### handle_packets: received packet for wrong port %u, when
                expecting %u\n", hdr.dport, Head->sport);
		return -1;
	}
	
	dprint("Handle packet: dealing with %s state\n", state_names[cc->state]);
	
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
		dprint("Not planned for this state yet :-( %s\n",
                state_names[cc->state]);
		break;

	}

	/* freeing up the memory alloted by recv_tcp_packet() */
	free(dat.content);
	return 1;
}

/*
 * handle_Listen_state : should expect syn packet should send syn+ack packet
 */
int 
handle_Listen_state(Header* hdr, Data* dat)
{
	TCPCtl* cc;
	int     flags;

	cc = Head->this;
	flags = hdr->flags;

	if (!(flags & SYN)) {
		/*
		 * didn't get expected packet... so, tolerating garbage
		 * packet hence, not changing the state*
		 */
		dprint("handle_Listen_state: Got wrong packet, ignoring it\n");
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
	
	dprint("handle_Listen_state: Got Syn, sending SYN+ACK\n");
	send_ack(SYN);

	return 1;
}

/*
 * handle_Syn_Sent_state : will be called from connect should expect syn+ack
 * packet and should send ack packet
 */
int 
handle_Syn_Sent_state(Header* hdr, Data* dat)
{
    TCPCtl* cc;
	int     flags;
	
	cc = Head->this;
	flags = hdr->flags;

	dprint("handle_Syn_Sent_state: inside function\n");
	if (!(flags & SYN)) {
		/*
		 * didn't get expected packet... so, tolerating garbage
		 * packet hence, not changing the state
		 */
		dprint("handle_Syn_Sent_state: got other dan SYN packet, ignoring\n");
		return -1;
	}
	
	/*
	 * ok, its SYN packet check if there is ack with it.
	 */
	if (flags & ACK) {
		/* check if ackno received is correct or not */
		if (cc->local_seqno + 1 != hdr->ackno) {
			/* got wrong ack-no... so, ignoring the packet */
			dprint("handle_Syn_Sent_state: got SYN + ACK packet, but wrong seq
                    no, ignoring it\n");
			return -1;
		}
		dprint("handle_Syn_Sent_state: got SYN + ACK packet, Acking the
                SYN\n");

		/* got correct ackno... so, updating variables */
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
	 * got just SYN packet it's a case of simultaneous OPEN
	 */
	dprint("handle_Syn_Sent_state: got just SYN, parallel open sending
            SYN+ACK\n");
	cc->state = Syn_Recv;
	cc->remote_seqno = hdr->seqno + 1;
	cc->remote_window = hdr->window;
	
	send_ack(0);
	return 1;
}


int 
handle_Syn_Recv_state(Header* hdr, Data* dat)
{
    TCPCtl* cc;
	int     flags;
	
	cc = Head->this;

	dprint("handle_Syn_Recv_state: inside function\n");
	flags = hdr->flags;

	if (!(flags & ACK)) {
		/*
		 * didn't get expected packet... so, tolerating garbage
		 * packet hence, not changing the state
		 */
		dprint("handle_Syn_Recv_state: didn't get ACK, so ignoring packet\n");
		/*
		 * checking if it's retransmisson SYN packet, because SYN+ACK
		 * was dropped
		 */
		return (handle_Listen_state(hdr, dat));
	}
	
	/*
	 * FIXME : you may get FIN packet at this stage... then you need to
	 * change to FIN_WAIT_1 state
	 */

	/* got ACK, check if it is corrct ACK */
	if (cc->local_seqno + 1 != hdr->ackno) {
		/* got wrong ack-no... so, ignoring the packet */
		dprint("handle_Syn_Recv_state: got ACK, bt with wrong ACK no,
		        ignoring packet\n");
		return -1;
	}
	
	/* ok, got ACK for my SYN, so moving to Established state */
	cc->remote_ackno = hdr->ackno;
	cc->local_seqno = hdr->ackno;
	cc->remote_seqno = hdr->seqno + dat->len;
	cc->remote_window = hdr->window;
	
	/* need to send ack for this */
	cc->state = Established;
	dprint("handle_Syn_Recv_state: got ACK, going to Established state\n");
	
	/* check if we have enough space in incoming buffer */
	if (dat->len > 0) {
		dprint("handle_Syn_Recv_state: got data also in ACK packet\n");
		if (cc->in_buffer->len + dat->len <= DATA_SIZE) {
			/*
			 * there is data in this packet, and also there is
			 * space in buffer so, put this data in buffer
			 */
			dprint("Length: %d\n", dat->len);
			memcpy(cc->in_buffer->content, dat->content, dat->len);
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
int 
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
		dprint("00000 handle_Established_state: Received unexpected packet,
                expected %u, recived %u, ACKing with old ACK number\n",
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
		dprint("Length: %d\n", dat->len);
		memcpy(cc->in_buffer->content, dat->content, dat->len);
		cc->in_buffer->len += dat->len;

		cc->remote_seqno = hdr->seqno + dat->len;

		/*
		 * if packet contain data or if it's fin packet, then only
		 * acknowledge it
		 */
		if (dat->len != 0) {
			if (hdr->flags & FIN) {
				dprint("ACKing the FIN packet (packet had data %d) with ACK no
                        %u\n", dat->len, cc->remote_seqno);
			}
			send_ack(0);
		} else {
			if (hdr->flags & FIN) {
				/* as FIN flag is 1 bit of data */
				/* cc->remote_seqno = hdr->seqno + 1 ; */
				cc->remote_seqno = hdr->seqno;
				dprint("ACKing the FIN packet with ACK no %u\n",
                        cc->remote_seqno + 1);
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
					dprint("handle_Established_state: some state issue,
                            unexpeced FIN flag received in state %d\n",
                            cc->state);
				}

			} else {
				dprint("Simple ACK packet, not acking it\n");
			}
		}
	} else {	
	    /* no buffer to store data, discard the
		 * packet send the ack with old ack number
		 * and with smaller window size */
		dprint("Packet dropped as lack of buffer: sending with old ACK No\n");
		send_ack(0);
	}

	return 1;
}


/* Support functions */
int 
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
		dprint("send_ack: sending SYN+ACK\n");
		hdr.flags = 0;
		hdr.flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT;
		hdr.flags = hdr.flags | SYN | ACK;
	} else {
		if (flags & FIN) {
			hdr.ackno = hdr.ackno + 1;
			dprint("send_ack: sending ACK for FIN\n");
		} else {
			dprint("send_ack: sending simple ACK\n");
		}
	}
	bytes_sent = _send_tcp_packet(&hdr, &dat);
	dprint("send_ack: sent....\n\n");
	
	free(dat.content);
	return 1;
}


/* for debugging support */
int 
noprint(char *fmt,...)
{
	return 0;
}

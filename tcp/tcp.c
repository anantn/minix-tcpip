#include "tcp.h"

/* Low level send and receive functions */
int
send_tcp_packet(Header* hdr, Data* dat)
{   
    uchar* tmp;
    int len, csum;
    
    if (!hdr->dst) {
        return 0;
    }
    
    hdr->src = my_ipaddr;
    hdr->prot = htons(IP_PROTO_TCP);
    hdr->tlen = htons(HEADER_SIZE + dat->len);
    swap_header(hdr, 0);
    
    tmp = (uchar*)calloc(sizeof(Header) + dat->len, sizeof(uchar));
    memcpy(tmp, (uchar*)hdr, sizeof(Header));
    memcpy(tmp + sizeof(Header), dat->content, dat->len);
    csum = raw_checksum(tmp, sizeof(Header) + dat->len);
    memcpy(tmp + CHECK_OFF, (uchar*)&csum, sizeof(u16_t));

    len = ip_send(hdr->dst, IP_PROTO_TCP, 2,
                    (void*)(tmp + HEADER_OFF), HEADER_SIZE + dat->len);
    free(tmp);
    
    return len;
}

int
recv_tcp_packet(Header* hdr, Data* dat)
{
    char* data;
    uchar* tmp;
    int len, doff;
    ushort prot, idp;
    ipaddr_t src, dst;
    
    len = ip_receive(&src, &dst, &prot, &idp, &data);

    /* Extract header - pseudo and real, still in network order */
    hdr->src = src;
    hdr->dst = dst;
    hdr->prot = htons(prot);
    hdr->tlen = htons(len);
    memcpy(((uchar*)hdr) + HEADER_OFF, data, HEADER_SIZE);
    
    /* Check the checksum! */
    tmp = (uchar*)calloc(HEADER_OFF + len, sizeof(uchar));
    memcpy(tmp, (uchar*)hdr, HEADER_OFF);
    memcpy(tmp + HEADER_OFF, (uchar*)data, len);
    if (raw_checksum(tmp, HEADER_OFF + len) != 0) {
        free(tmp);
        dprint("Incorrect Checksum! Discarding packet...\n");
        return -1;
    }
    free(tmp);
    
    /* Back to host order */
    hdr->prot = prot;
    hdr->tlen = len;
    swap_header(hdr, 1);
    
    /* We ignore options, just interested in data offset
       (which is described in number of WORD_SIZE bytes).
     */
    doff = (hdr->flags >> DATA_SHIFT) * WORD_SIZE;
    dat->len = len - doff;    
    dat->content = (uchar*)calloc(dat->len, sizeof(uchar));
    memcpy(dat->content, (uchar*)data + doff, dat->len);
    
    return len;
}

/* Init TCP: Single connection for now */
int
tcp_socket()
{
    TCPCtl* ctl;
    if (!ip_init()) {
//		return 0;
	}

    Head = (TCPMux*)calloc(1, sizeof(TCPMux));
    ctl = (TCPCtl*)calloc(1, sizeof(TCPCtl));

	// allocating memory for incomming buffer
	ctl->in_buffer = (Data*)calloc(1, sizeof(Data));
	ctl->in_buffer->content = (uchar*)calloc(DATA_SIZE, sizeof(uchar));
	memset (ctl->in_buffer, 0, (DATA_SIZE)*sizeof(uchar));
	ctl->in_buffer->len = 0 ;

	// allocating memory for outgoing buffer
	ctl->out_buffer = (Data*)calloc(1, sizeof(Data));
	ctl->out_buffer->content = (uchar*)calloc(DATA_SIZE, sizeof(uchar));
	memset (ctl->out_buffer, 0, (DATA_SIZE)*sizeof(uchar));
	ctl->out_buffer->len = 0 ;
		
	// clear the data buffer
	ctl->remaining = 0 ;
       
	ctl->state = Closed ;
    Head->socket = 1;
    Head->sport = 9090;
    Head->this = ctl;
    Head->next = NULL;
       
    return 1;
}


/* Connection oriented part */



int
tcp_connect (ipaddr_t dst, int port )
{
	TCPCtl * cc ; // current_connection
	cc = Head->this ;
	/* Do three way handshake */

	/* make sure that tcp_socket is called before calling this function */
	
	// now change the state
	cc->type = TCP_CONNECT ;
	cc->state = Established ;
	cc->local_seqno = 10 ;
	cc->remote_ackno = 10 ;
	cc->remote_seqno = 1;
	
	Head->dport = port ;
	Head->dst = dst ;

	return 1 ; // return proper value
} // end function : tcp_connect

/* tcp listen : */
int
tcp_listen (int port, ipaddr_t *src)
{
	TCPCtl * cc ; // current_connection
	cc = Head->this ;
	/* Do three way handshake */

	/* make sure that tcp_socket is called before calling this function */
	
	// now change the state
	cc->type = TCP_LISTEN ;
	cc->state = Established ;
	cc->local_seqno = 1 ;
	cc->remote_ackno = 1;
	cc->remote_seqno = 10; 

	return 1 ; // return proper value
} // end function : tcp_listen ()

int
tcp_read (char *buf, int maxlen )
{
	int remaining_bytes = 0 ;
	int bytes_read = 0 ;
	TCPCtl * cc ; // current_connection
	cc = Head->this ;


	while (cc->in_buffer->len == 0 )
	{
		// check if the connection from other side is closed...
		// if yes, then return EOF
		dprint ("tcp_read: no data in read buffer, calling handle packets\n");	
		handle_packets ();
	} // end while : buffer empty

	bytes_read = MIN (cc->in_buffer->len, maxlen );
	// copy the bytes into buf given by user
	memcpy (buf, cc->in_buffer->content, bytes_read );
	remaining_bytes = cc->in_buffer->len - bytes_read ;

	if (remaining_bytes > 0 )
	{
		memmove(cc->in_buffer->content, (cc->in_buffer->content+bytes_read), remaining_bytes);
		cc->in_buffer->len = remaining_bytes ;
	}
	else
	{
		// clear the data buffer
		memset (cc->in_buffer->content, 0, (DATA_SIZE)*sizeof(uchar));
		cc->in_buffer->len = 0 ;
	}


	return bytes_read ;
} // end function : tcp_read ()


int
tcp_write (char * buf, int len )
{
	int bytes_left ;
	int bytes_sent ;
	int packet_size ;
	bytes_left = len ;

	dprint ("tcp_write: writing %d bytes\n", len);
	while (bytes_left > 0 )
	{
		// check if the connection from your side is closed...
		// if yes, then return error
		packet_size = MIN (DATA_SIZE, bytes_left ) ;
		dprint ("tcp_write: writing %d bytes of packet by calling write_packet\n", packet_size);
		bytes_sent = write_packet (buf, packet_size ) ;

		// upgrade the code for supporting multiple writes to send all data
		return bytes_sent ;

	} // end while : bytes left
	return bytes_sent ;
	
} // end function : tcp_write ()

// private function
int
write_packet (char * buf, int len )
{
	TCPCtl * cc ; // current_connection
	Header hdr ;
	Data dat ;
	int bytes_sent ;
	int ack_no ;

	cc = Head->this ;

	// assign data
	dat.content = buf ;
	dat.len = len ;

	// setup packet
	setup_packet (&hdr);

	// send the packet
	do
	{
		dprint ("write_packt: sending tcp packet\n");
		bytes_sent = send_tcp_packet (&hdr, &dat);
		dprint ("write_packt: waiting for ack\n");
		ack_no = wait_for_ack () ;
	}
	while (ack_no != (cc->local_seqno + len )  );
	dprint ("write:packet:Packet sent successfully\n");
	return len ;
} // end function : write_packet

int wait_for_ack ()
{
	int ack_no ;
	TCPCtl * cc ; // current_connection
	cc = Head->this ;

	dprint ("wait_for_ack: calling handle_packet\n");
	handle_packets () ;
	ack_no = cc->remote_ackno ;
	return (ack_no) ;
		
}

int setup_packet (Header *hdr )
{
	TCPCtl * cc ; // current_connection
	cc = Head->this ;
//	hdr->src = 	// set at IP layer 
//	hdr->prot = // set at ip layer
//	hdr->tlen = //set at IP layer
	
	hdr->dst = Head->dst ;  
	hdr->sport = Head->sport ;
	hdr->dport = Head->dport ;
	hdr->seqno = cc->local_seqno ; 
	// when to increase the local sequence no..??
	// now ?? or when we get ack ??
	hdr->ackno = cc->remote_seqno ;
	hdr->flags = 0 ;
	hdr->window = 1 ;
	hdr->urgent = 1 ;
	// setup flags ;
	hdr->flags = 0 ; // clearing flags first
	hdr->flags = URG | ACK | PSH ;
	hdr->flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT; // why..???

	return 1 ; 
} // end function : setup_packet


int
tcp_close (void)
{
	TCPCtl * cc ; // current_connection
	cc = Head->this ;
	cc->state = Closed ;
	
	return 1 ;
} // end function : tcp_close()


// waits for one incomming packet
// put the data in incoming buffer
// and update the ack, remote seq no.
int
handle_packets ()
{
	Header hdr ;
	Data dat ;
	TCPCtl * cc ; // current_connection
	int len ;

	cc = Head->this ;

	dprint ("handle_packet: waiting for packet\n" );
	dat.content = (uchar*)calloc(DATA_SIZE, sizeof(uchar));
	memset (dat.content, 0, DATA_SIZE);
	do
	{
		len = recv_tcp_packet (&hdr, &dat) ;
		dprint ("handle_packet: received packet with len %d\n", len );
	} while (len < 0 );

	// update the ack number received
	cc->remote_ackno = hdr.ackno ;

	// check if u have enough space in incoming buffer
	if (cc->in_buffer->len + len < DATA_SIZE )
	{
		// accept the packet
		memcpy(cc->in_buffer->content + cc->in_buffer->len, dat.content, len);	
		cc->in_buffer->len += len;
		cc->remote_seqno = hdr.seqno + len ;
		send_ack ();
	}
	else
	{
		// discard the packet
		// send the ack with old ack number and window size 0	
	}

	// check for other flags, specially finish flag
	return 1 ; // return something meaningfull
}

int send_ack ()
{
	Data dat ;	
	Header hdr ;
	int bytes_sent ;

	dat.content = (uchar*)calloc(DATA_SIZE, sizeof(uchar));
	memset (dat.content, 0, DATA_SIZE);
	dat.len = 0 ;
	setup_packet (&hdr);
	bytes_sent = send_tcp_packet (&hdr, &dat);
	return 1 ; 

}

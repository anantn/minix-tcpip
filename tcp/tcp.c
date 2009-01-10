#include "tcp.h"

TCPMux* Head;

/* Low level send and receive functions */
int
send_tcp_packet(Header* hdr, Data* dat)
{   
    uchar* tmp;
    int len, csum;
    
    if (!hdr->dst) {
		dprint("send_tcp_packet: Error: Destination not specified ");
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
/*	// For detailed Debugging
	dprint("Checksum calcuated (Network order): %x\n", csum);
	dprint("Packet sending : header and data dumped\n" );
	dump_header(hdr);
	dump_buffer(dat->content, dat->len);
*/
    memcpy(tmp + CHECK_OFF, (uchar*)&csum, sizeof(u16_t));

    len = ip_send(hdr->dst, IP_PROTO_TCP, 2,
                    (void*)(tmp + HEADER_OFF), HEADER_SIZE + dat->len);
    free(tmp);
    
    return dat->len;
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
        dprint("Incorrect Checksum! Discarding packet...");
		dprint("Got: %x instead of 0\n", raw_checksum(tmp, HEADER_OFF + len));
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
	// this memory is to be freed by calling function
	// it's freed in handle_packets function
    memcpy(dat->content, (uchar*)data + doff, dat->len);
/*	// For detailed Debugging
	dprint ("packet received : Printing header and data\n");
	dump_header(hdr);
	dump_buffer(dat->content, dat->len);
*/    
    return dat->len;
}

/* Init TCP: Single connection for now */
int
tcp_socket()
{
	TCPCtl * cc ; // current_connection


    TCPCtl* ctl;
    if (!ip_init()) {
//		return 0;
	}

    Head = (TCPMux*)calloc(1, sizeof(TCPMux)); // FIXME :need to create linked list
    ctl = (TCPCtl*)calloc(1, sizeof(TCPCtl));

	// all memory alloted here will be freed in socket_close call.
	
	// allocating memory for incomming buffer
	ctl->in_buffer = (Data*)calloc(1, sizeof(Data));
	ctl->in_buffer->content = (uchar*)calloc(DATA_SIZE, sizeof(uchar));
	memset (ctl->in_buffer->content, 0, (DATA_SIZE)*sizeof(uchar));
	ctl->in_buffer->len = 0 ;

	// allocating memory for outgoing buffer
	ctl->out_buffer = (Data*)calloc(1, sizeof(Data));
	ctl->out_buffer->content = (uchar*)calloc(DATA_SIZE, sizeof(uchar));
	memset (ctl->out_buffer->content, 0, (DATA_SIZE)*sizeof(uchar));
	ctl->out_buffer->len = 0 ;
		
	// clear the data buffer
	ctl->remaining = 0 ;
       
	ctl->state = Closed ;
    Head->socket = 1;
    Head->sport = 9090;
    Head->this = ctl;
    Head->next = NULL;
       
	cc = Head->this ;

	cc->local_seqno = 100 ; // FIXME : better way to set initial seq-no ..???
	cc->remote_ackno = 0 ; // has to be set in three way handshake
	cc->remote_seqno = 0 ;  // has to be set in three way handshake
	cc->remote_window = DATA_SIZE ;  // has to be set in three way handshake
    return 1; // FIXME : checkup for failure of tcp_socket
}


/* Connection oriented part */


int
tcp_connect (ipaddr_t dst, int port )
{
	TCPCtl * cc ; // current_connection
	cc = Head->this ;
	/* Do three way handshake */
	Header hdr ;
	Data dat ;
	int bytes_sent ;

	/* make sure that tcp_socket is called before calling this function */

	// clear variables
	memset (&hdr, 0, sizeof (hdr));
	memset (&dat, 0, sizeof (dat));

	dat.content = (uchar*)calloc(DATA_SIZE, sizeof(uchar));
	dat.len = 0 ;

	// setting up status variables for this connection
	cc->type = TCP_CONNECT ;
	cc->local_seqno = 100 ; // FIXME : better way to set initial seq-no ..???
	Head->dport = port ;
	Head->dst = dst ;
	Head->sport = 6000 ; // FIXME : better way to assign local port
	cc->state = Syn_Sent ; 

	// setup packet
	setup_packet (&hdr);

	// resetting the flags to be the first SYN packet
	hdr.flags = 0 ; // clearing flags first
	hdr.flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT;   
	hdr.flags = hdr.flags | SYN ; // setup default flags

	// now, send the packet
	bytes_sent = send_tcp_packet (&hdr, &dat);
	
	free (dat.content) ;

	if (bytes_sent == -1 )
	{
		dprint ("Error: send_tcp_packet failed\n");	
		exit(1);
	}
	do
	{
		handle_packets ();
	}
	while (cc->state != Established ); 

	return 1 ; // FIXME:return proper value

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
	cc->state = Listen ; 
	cc->local_seqno = 800 ; // FIXME : better way to set initial seq-no ..???

	do
	{
		handle_packets ();
	}
	while (cc->state != Established ); 

	return 1 ; // FIXME:return proper value



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
	}
	cc->in_buffer->len = remaining_bytes ;
	// clear the data buffer
	memset (cc->in_buffer->content + remaining_bytes, 0, (DATA_SIZE-remaining_bytes)*sizeof(uchar));

	return bytes_read ;
} // end function : tcp_read ()


int
tcp_write (char * buf, int len )
{
	int bytes_left ;
	int bytes_sent ;
	int packet_size ;
	TCPCtl * cc ; // current_connection
	cc = Head->this ;

	bytes_left = len ;

	dprint ("tcp_write: writing %d bytes\n", len);
	while (bytes_left > 0 )
	{
		// check if the connection from your side is closed...
		// if yes, then return error
		packet_size = MIN (cc->remote_window, bytes_left ) ;
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

	// clear variables
	memset (&hdr, 0, sizeof (hdr));
	memset (&dat, 0, sizeof (dat));
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
	//FIXME :need to worry about overflowing in comparision 
	dprint ("write:packet:Packet sent successfully\n");
	cc->local_seqno = cc->local_seqno + len ; // updating local seq no.
	//FIXME :need to worry about overflowing
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
	hdr->ackno = cc->remote_seqno ;
	dprint ( "window size = %d - %d", DATA_SIZE, cc->in_buffer->len);
	hdr->window = DATA_SIZE - cc->in_buffer->len ;
	dprint ( " = %d\n", hdr->window );
	hdr->urgent = 1 ;
	// setup flags ; FIXME : need to fix this part
	hdr->flags = 0 ; // clearing flags first
	hdr->flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT; //  
	hdr->flags = hdr->flags | URG | ACK | PSH ; // setup default flags

	return 1 ; 
} // end function : setup_packet

int
tcp_close (void)
{

	TCPCtl * cc ; // current_connection
	cc = Head->this ;

	// send FIN and start 4-way closing procedure
	return 1 ;
}

// A function which will release all resources alloted to socket
int
socket_close (void)
{
	TCPCtl * cc ; // current_connection
	cc = Head->this ;
	cc->state = Closed ;
	free (cc->in_buffer->content);
	free (cc->out_buffer->content);
	free (cc->in_buffer);
	free (cc->out_buffer);

	
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
	int len, ret ;
	TCPCtl * cc ; // current_connection
	cc = Head->this ;

	dprint ("handle_packet: state = %d, waiting for packet\n", cc->state  );
	do
	{
		len = recv_tcp_packet (&hdr, &dat) ;
		dprint ("handle_packet: received packet with len %d\n", len );
	} while (len < 0 );

	switch (cc->state )
	{
		case Closed : dprint("Handle packet: dealing with closed state\n");
						ret = handle_Closed_state (&hdr,&dat) ;
						break ;

		case Listen : dprint("Handle packet: dealing with Listen state\n");
						ret = handle_Listen_state (&hdr, &dat);
						break ;

		case Syn_Sent : dprint("Handle packet: dealing with Syn Sent state\n");
						ret = handle_Syn_Sent_state (&hdr, &dat);
						break ;

		case Syn_Recv : dprint("Handle packet: dealing with syn_recv state\n");
						ret = handle_Syn_Recv_state (&hdr, &dat);
						break ;

		case Established:dprint("Handle packet: dealing with established state\n");
						ret = handle_Established_state (&hdr, &dat);
						break ;

		default :	dprint ("Not planned for this state yet :-( \n"); 
						break ;

	} // end switch : connection state
	// freeing up the memory alloted by recv_tcp_packet()
	free (dat.content);
	// FIXME : return something meaningfull
	return 1 ; 
}

int handle_Closed_state (Header *hdr, Data *dat)
{
	return 1;	
}

// handle_Listen_state : should expect syn packet
// should send syn+ack packet
int handle_Listen_state (Header *hdr, Data *dat)
{
	int flags ;
	TCPCtl * cc ; // current_connection
	cc = Head->this ;

	flags = hdr->flags ;

	if (!( flags & SYN ) ) 
	{
		// didn't get expected packet...
		// so, tolerating garbage packet
		// hence, not changing the state
		dprint ("handle_Listen_state: Got wrong packet, ignoring it\n");
		return -1 ;
	}
	// ok, its SYN packet
	

	// Get information about other side
	Head->dst = hdr->src ; 	
	Head->dport = hdr->sport ;
	cc->remote_seqno = hdr->seqno + 1; //FIXME :need to worry about overflowing
	cc->remote_window = hdr->window ; // updating remote window size
	cc->state = Syn_Recv ; 
	dprint ("handle_Listen_state: Got Syn, sending SYN+ACK\n");

	send_ack ();

	return 1;	
}

// handle_Syn_Sent_state : will be called from connect
// should expect syn+ack packet
// and should send ack packet
int handle_Syn_Sent_state  (Header *hdr, Data *dat)
{
	int flags ;
	TCPCtl * cc ; // current_connection
	cc = Head->this ;

	flags = hdr->flags ;

	dprint ("handle_Syn_Sent_state: inside function\n");
	if (!( flags & SYN ) ) 
	{
		// didn't get expected packet...
		// so, tolerating garbage packet
		// hence, not changing the state
		dprint ("handle_Syn_Sent_state: got other dan SYN packet, ignoring it\n");
		return -1 ;
	}
		
	// ok, its SYN packet
	// check if there is ack with it.
	if (flags & ACK )
	{
		// check if ackno received is correct or not
		if(cc->local_seqno+1 != hdr->ackno)//FIXME :need to worry about overflowing
		{
			// got wrong ack-no... so, ignoring the packet
		dprint ("handle_Syn_Sent_state: got SYN + ACK packet, bt wrong seq no, ignoring it\n");
			return -1 ;	
		}

		dprint ("handle_Syn_Sent_state: got SYN + ACK packet, Acking the SYN\n");
		// got correct ackno... so, updating variables
		cc->remote_ackno = hdr->ackno ;
		cc->local_seqno = hdr->ackno ;
		cc->remote_seqno = hdr->seqno + 1; //FIXME :need to worry about overflowing
		cc->remote_window = hdr->window ; // updating remote window size
		// need to send ack for this
		cc->state = Established ; 
		send_ack ();
		return 1 ;
	}
	// got just SYN packet
	// it's a case of simultaneous OPEN
	dprint ("handle_Syn_Sent_state: got just SYN, parallel open sending SYN+ACK\n");
	cc->state = Syn_Recv ; 
	cc->remote_seqno = hdr->seqno + 1; //FIXME :need to worry about overflowing
	cc->remote_window = hdr->window ; // updating remote window size
	send_ack ();
	
	return 1;	
}


int handle_Syn_Recv_state (Header *hdr, Data *dat)
{
	int flags ;
	TCPCtl * cc ; // current_connection
	cc = Head->this ;

	dprint ("handle_Syn_Recv_state: inside function\n");
	flags = hdr->flags ;

	if (!( flags & ACK ) ) 
	{
		// didn't get expected packet...
		// so, tolerating garbage packet
		// hence, not changing the state
	dprint ("handle_Syn_Recv_state: didn't get ACK, so ignoring packet\n");
		return -1 ;
	}
	// got ACK, check if it is corrct ACK
	if(cc->local_seqno+1 != hdr->ackno)//FIXME :need to worry about overflowing
	{
		// got wrong ack-no... so, ignoring the packet
	dprint ("handle_Syn_Recv_state: got ACK, bt with wrong ACK no, ignoring packet\n");
		return -1 ;	
	}
	
	// ok, got ACK for my SYN, so moving to Established state
	cc->remote_ackno = hdr->ackno ;
	cc->local_seqno = hdr->ackno ;
//	cc->remote_seqno = hdr->seqno + 1; //FIXME :need to worry about overflowing
	cc->remote_window = hdr->window ; // updating remote window size
	// need to send ack for this
	cc->state = Established ; 
	dprint ("handle_Syn_Recv_state: got ACK, going ot Established state\n");
	return 1;	
}

// handle_Established_state :
// you can receive following packets
// 1. data packets
// 2. ack packets
// 3. fin packets
int handle_Established_state (Header *hdr, Data *dat)
{


	TCPCtl * cc ; // current_connection
	cc = Head->this ;

	// update the ack number received
	cc->remote_ackno = hdr->ackno ;
	cc->remote_window = hdr->window ; // updating remote window size

	// check if u have enough space in incoming buffer
	if (cc->in_buffer->len + dat->len < DATA_SIZE )
	{
		// accept the packet
		dprint("Length: %d\n", dat->len);
		memcpy(cc->in_buffer->content, dat->content, dat->len);	
		cc->in_buffer->len += dat->len;


		cc->remote_seqno = hdr->seqno + dat->len ; //FIXME :need to worry about overflowing

		// if packet contain data or if it's fin packet,
		// then only acknowledge it 
		if (dat->len != 0 )
		{
			send_ack ();
		}
	}
	else
	{	// no buffer to store data, 
		// discard the packet
		// send the ack with old ack number and
		// with smaller window size

		// if packet contain data or if it's fin packet,
		// then only acknowledge it 
		if (dat->len != 0 )
		{
			send_ack ();
		}
	}
	// FIXME : return something meaningfull
	return 1;	
}


int send_ack ()
{
	Data dat ;	
	Header hdr ;
	int bytes_sent ;
	TCPCtl * cc ; // current_connection
	cc = Head->this ;

	dat.content = (uchar*)calloc(DATA_SIZE, sizeof(uchar));
	memset (dat.content, 0, DATA_SIZE);
	memset (&hdr, 0, sizeof (hdr));
	dat.len = 0 ;
	setup_packet (&hdr);

	if ( cc->state == Syn_Recv )
	{
		// need to send SYN+ACK
		
		dprint("send_ack: sending SYN+ACK\n");
		// setup flags ; FIXME : need to fix this part
		hdr.flags = 0 ; // clearing flags first
		hdr.flags = (HEADER_SIZE / WORD_SIZE) << DATA_SHIFT; //  
		hdr.flags = hdr.flags | SYN | ACK ; // setup default flags
	}
	else
	{
		dprint("send_ack: sending simple ACK\n");
	}
	bytes_sent = send_tcp_packet (&hdr, &dat);
	dprint("send_ack: sent....\n\n");
	free (dat.content);
	return 1 ; 
} // end function : send_ack 

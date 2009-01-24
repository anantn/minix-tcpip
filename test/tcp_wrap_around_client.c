#include <tcp.h>

/* To run this program 
 ETH=2 ./tcp_wrap_around_client
 * */

int
main()
{
    char* buf;
	TCPCtl *cc ;
    
    tcp_socket();
	cc = Head->this ;
	/* changing seq-no to very big seq no.*/
	/* The biggest possible seqno is  4294967296 */
	/* so, taking value just bellow it */
	cc->local_seqno =  4294967296 - 110 ;
	/* subtracting 110 because, local_seqno is incremented by 100 in connect call*/
    tcp_connect(inet_addr("192.168.0.1"), 1080);
    
    buf = (char*)calloc(10000, sizeof(char));
    memset(buf, '1', 9999);
    
    tcp_write(buf, 100);
    tcp_close();
    
    return 1;
}

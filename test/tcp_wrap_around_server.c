#include <tcp.h>

/* To run this program 
 ETH=1 ./tcp_wrap_around_server
 * */

int
main()
{
    int i, j;
    char* buf;
	TCPCtl *cc ;
    
    tcp_socket();
	cc = Head->this ;
	/* changing seq-no to very big seq no.*/
	/* The biggest possible seqno is  4294967296 */
	/* so, taking value just bellow it */
	cc->local_seqno =  4294967296 - 415 ;
	/* subtracting 415 because, local_seqno is incremented by 400 in listen call*/

    tcp_listen(1080, (ipaddr_t*)my_ipaddr);
    
    buf = (char*)calloc(1025, sizeof(char));
    
    i = 0;
    j = tcp_read(buf, 1024);
    while (j != EOF) {
        printf("%s", buf);
        i += j;
        j = tcp_read(buf, 1024);
    }
    printf("\n");
    i += j;
    printf("%d bytes read!\n\n", i);
    
    tcp_close();
    return 1;
}

#include <tcp.h>

/* To run this program 
 ETH=1 ./tcp_generic_server
 * */

int
main()
{
    int i, j;
    char* buf;
    
    tcp_socket();
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

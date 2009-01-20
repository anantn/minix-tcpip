#include <tcp.h>

int
main()
{
    char* buf;
    
    tcp_socket();
    tcp_connect(inet_addr("192.168.0.1"), 1080);
    
    buf = (char*)calloc(10000, sizeof(char));
    memset(buf, '1', 9999);
    
    tcp_write(buf, 10000);
    tcp_close();
    
    return 1;
}

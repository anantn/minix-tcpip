#include <tcp.h>

int
parse_url(char* url, char* host, char* path)
{
    char* found = strchr(url + 7, '/');
    
    if (!found) {
        return 0;
    }
}

int
main(int argc, char** argv)
{    
    if (argc != 2) {
        /* Invalid arguments passes */
        dprint("Usage: httpc <url>\n");
        return 1;
    }
    
    if (strncmp(argv[1], "http://", 7)) {
        /* Non-HTTP protocols not supported */
        dprint("Only http:// supported, quitting!\n");
        return 1;
    }
}


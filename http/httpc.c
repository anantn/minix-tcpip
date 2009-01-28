#include <tcp.h>

int
parse_url(char* url, char* host, char* path)
{
    int hl;
    char* found = strchr(url + 7, '/');
    
    if (!found) {
        return 0;
    }
    
    hl = (int)found - ((int)url + 7);
    host = (char*) calloc(hl + 1, sizeof(char));
    path = (char*) calloc(strlen(found), sizeof(char));
    
    strcpy(path, found + 1);
    strncpy(host, url + 7, hl);
    
    dprint("httpc:: Fetching %s from %s\n", path, host);
    return 1;
}

int
fetch(char* path, char* host)
{
    return 0;
}

int
main(int argc, char** argv)
{
    char* host;
    char* path;
    
    if (argc != 2) {
        /* Invalid arguments passes */
        dprint("httpc:: Usage: httpc <url>\n");
        return 1;
    }
    
    if (strncmp(argv[1], "http://", 7)) {
        /* Non-HTTP protocols not supported */
        dprint("httpc:: Only http:// supported, quitting!\n");
        return 1;
    }
    
    if (parse_url(argv[1], host, path)) {
        dprint("httpc:: Fetching %s from %s\n", path, host);
        return fetch(path, host);
    } else {
        dprint("httpc:: Invalid host, quitting!\n");
        return 1;
    }
}

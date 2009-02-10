#include <http.h>

int
print_status(char* buf)
{
    printf("The return code is: ");
    if (strncmp(buf + 9, "200", 3) == 0) {
        printf("200 OK\n");
        return 1;
    } else if (strncmp(buf + 9, "404", 3) == 0) {
        printf("404 Not Found\n");
        return 0;
    } else if (strncmp(buf + 9, "403", 3) == 0) {
        printf("403 Forbidden\n");
        return -1;
    }
    
    return -2;
}

void
print_headers(char* buf)
{
    char sneak[4];
    char content[5];
    char modified[9];
    
    tcp_reliable_read(sneak, 4);
    
    while (sneak[0] != '\r' && sneak[1] != '\n') {
        if (strncmp(sneak, "Date", 4) == 0) {
            printf("The date of retrieval is");
            chomp(1);
        } else if (strncmp(sneak, "Cont", 4) == 0) {
            tcp_reliable_read(content, 5);
            if (strncmp(content, "ent-L", 5) == 0) {
                tcp_reliable_read(content, 5);
                printf("The size of the document is");
                chomp(1);
            } else if (strncmp(content, "ent-T", 5) == 0) {
                tcp_reliable_read(content, 3);
                printf("The Mime type of the document is");
                chomp(1);
            } else {
                chomp(0);
            }
        } else if (strncmp(sneak, "Last", 4) == 0) {
            tcp_reliable_read(modified, 9);
            if (strncmp(modified, "-Modified", 9) == 0) {
                printf("The document was last modified on");
                chomp(1);
            } else {
                chomp(0);
            }
        } else {
            chomp(0);
        }
        
        tcp_reliable_read(sneak, 4);
    }
    
    buf[0] = sneak[2]; buf[1] = sneak[3];
}

int
parse_url(char* url, char** host, char** path)
{
    int hl;
    char* found = strchr(url + 7, '/');
    
    if (!found) {
        return 0;
    }
    
    hl = (unsigned long)found - ((unsigned long)url + 7);
    *host = (char*) calloc(hl + 1, sizeof(char));
    *path = (char*) calloc(strlen(found), sizeof(char));
    
    strcpy(*path, found + 1);
    strncpy(*host, url + 7, hl);
    return 1;
}

void
handle_response(char* path)
{
    FILE* f;
    int read;
    char* buf;
    
    f = fopen(path, "w+");
    if (!f) {
        dprint("httpc:: Could not create file for output, aborting!\n");
        return;
    }
    
    buf = (char*) calloc(32, sizeof(char));
    chomp(0);
    print_headers(buf);

    fwrite(buf, sizeof(char), 2, f);
    while ((read = tcp_read(buf, 32)) != EOF) {
        fwrite(buf, sizeof(char), read, f);
    }
    fclose(f);
    
    free(buf);
}

int
fetch(char* path, char* host)
{
    char* req;
    int i, ret;
    ipaddr_t addr;
    struct hostent* h;
    
    if (tcp_socket() < 0) {
        printf("Could not initialize tcp_socket, quitting!\n");
        return 1;
    }
    
    addr = inet_addr(host);
    if (!addr) {
        /* Let's try name resolution */
        h = gethostbyname(host);
        if (!h) {
            printf("No such host %s found, quitting!\n", host);
            return 1;
        }
        addr = inet_addr(h->h_addr_list[0]);
        if (!h) {
            printf("Failed to resolved hostname %s, quitting!\n", host);
            return 1;
        }
    }
    
    if (!tcp_connect(addr, 80)) {
        printf("Could not connect to %s, quitting!\n", host);
        return 1;
    }
    
    req = (char*) calloc(18 + strlen(path), sizeof(char));
    sprintf(req, "GET /%s HTTP/1.0\r\n\r\n", path);
    dprint("httpc:: %s\n", req);
    
    tcp_write(req, 18 + strlen(path));
    
    /* Get response status */
    tcp_reliable_read(req, 13);
    i = print_status(req);
    free(req);
    
    switch (i) {
        case 1:
            handle_response(path);
            ret = 0;
            break;
        case 0:
            printf("The document could not be found on the server. ");
            printf("Perhaps the name of the document is incorrect?\n");
            ret = 1;
            break;
        case -1:
            printf("The server did not have permissions to access ");
            printf("the document!\n");
            ret = 1;
            break;
        case -2:
            printf("The server sent an incorrect HTTP response!\n");
            ret = 1;
            break;
    }
    
    free(path); free(host);
    return ret;
}

int
main(int argc, char** argv)
{
    char* host;
    char* path;
    
    if (argc != 2) {
        /* Invalid arguments passed */
        printf("Usage: httpc <url>\n");
        return 1;
    }
    
    if (strncmp(argv[1], "http://", 7)) {
        /* Non-HTTP protocols not supported */
        printf("Only http:// supported, quitting!\n");
        return 1;
    }
    
    if (parse_url(argv[1], &host, &path)) {
        dprint("httpc:: Fetching %s from %s\n", path, host);
        return fetch(path, host);
    } else {
        printf("Invalid host, quitting!\n");
        return 1;
    }
}

/* vim: set ts=4 sw=4 expandtab tw=78: */


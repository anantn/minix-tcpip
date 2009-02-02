#include <http.h>

void
get_mime_type(char* fPath, char* s)
{
    int len = strlen(fPath);
    
    if (strncmp("txt", fPath + len - 3, 3) == 0)
        strcpy(s, "text/plain");
    else if (strncmp("htm", fPath + len - 3, 3) == 0)
        strcpy(s, "text/HTML");
    else if (strncmp("html", fPath + len - 4, 4) == 0)
        strcpy(s, "text/HTML");
    else if (strncmp("gif", fPath + len - 3, 3) == 0)
        strcpy(s, "image/gif");
    else if (strncmp("jpg", fPath + len - 3, 3) == 0)
        strcpy(s, "image/jpg");
    else if (strncmp("jpeg", fPath + len - 4, 4) == 0)
        strcpy(s, "image/jpg");
    else if (strncmp("ps", fPath + len - 2, 2) == 0)
        strcpy(s, "application/postscript");
    else
        strcpy(s, "application/octet-stream");
}

void
send_initial_response(int code)
{   
    time_t t;
    tcp_write("HTTP/1.0 ", 9);
    switch (code) {
        case 404:
            tcp_write("404 Not Found\r\n", 15);
            break;
        case 403:
            tcp_write("403 Forbidden\r\n", 15);
            break;
        default:
            tcp_write("200 OK\r\n", 8);
            break;
    }

    time(&t);
    tcp_write("Date: ", 6);
    tcp_write(ctime(&t), 24);
    tcp_write("\r\n", 2);
    tcp_write("Server: CN Practical '08/09 (Minix 3.1)\r\n", 41);   
}

void
handle_get(char* htdocs)
{
    int i, j;
    time_t t;
    char len[10];
    char mime[24];
    char path[256];    
    char* fPath;
    FILE* f;
    unsigned char* contents;
    struct stat buf;
    char* ptr = path;
    
    i = 0;
    do {
        tcp_read(ptr, 1);
        ptr++; i++;
    } while ((*(ptr - 1) != 32) && (i < 256));

    fPath = (char*)calloc(strlen(htdocs) + i, sizeof(char));
    memcpy(fPath, htdocs, strlen(htdocs));
    memcpy(fPath + strlen(htdocs), path, i - 1);
    dprint("httpd:: File path determined: %s\n", fPath);
	dprint("httpd:: Flushing read buffer...\n");
    read_flush();

    if (stat(fPath, &buf) != 0) {
        /* Cannot access file because it does not exist */
		dprint("httpd:: Requested file does not exist, stat returned %s :(\n",
                strerror(errno));
        send_initial_response(404);
        tcp_write("\r\n", 2);
        free(fPath);
        return;
    }
    
    if (access(fPath, R_OK) != 0) {
        /* Do not have read access */
        dprint("httpd:: Do no have read access to the file, sending 403\n");
        send_initial_response(403);
        tcp_write("\r\n", 2);
        free(fPath);
        return;
    }
    
    contents = (unsigned char*) calloc(buf.st_size, sizeof(unsigned char));    
    
    f = fopen(fPath, "r+");
    fread(contents, sizeof(unsigned char), buf.st_size, f);
    fclose(f);
    
    /* Send response */
	dprint("httpd:: File found, sending headers and content\n");
    send_initial_response(200);
    
    /* Send Last-Modified */
    t = buf.st_mtime;
    tcp_write("Last-Modified: ", 15);
    tcp_write(ctime(&t), 24);
    tcp_write("\r\n", 2);
    
    /* Send Content-Length */
    tcp_write("Content-Length: ", 16);
    j = my_itoa(buf.st_size, len);
    tcp_write(len, j);
    tcp_write("\r\n", 2);
    
    /* Send MIME type */
    tcp_write("Content-Type: ", 14);
    get_mime_type(fPath, mime);
    tcp_write(mime, strlen(mime));
    tcp_write("\r\n", 2);
    
    /* Send contents */
    tcp_write("\r\n", 2);
    tcp_write((char*)contents, buf.st_size);
    
    free(fPath);
    free(contents);
}

int
serve(char* htdocs)
{
    char method[4];

    if (!tcp_socket()) {
        dprint("httpd:: Could not initialize tcp_socket, quitting!\n");
        return 1;
    }
    
    if (!tcp_listen(80, (ipaddr_t*)my_ipaddr)) {
        dprint("httpd:: Could not listen on port 80, quitting!\n");
        return 1;
    }
    
    /* First, let's find out which HTTP method is requested */
    tcp_read(method, 4);

    switch (method[0]) {
        case 'G':
            if (method[1] == 'E' && method[2] == 'T' && method[3] == ' ') {
                handle_get(htdocs);
                tcp_close();
                return 0;
            }
        default:
            /* Other HTTP methods not supported */
            dprint("httpd:: Method %s not supported, quitting!\n", method);
            tcp_close();
            return 1;
    }
}

int
main(int argc, char** argv)
{
    DIR* check;

    if (argc != 2) {
        /* Invalid arguments passed */
        dprint("httpd:: Usage: httpd <directory>\n");
        return 1;
    }
    
    check = opendir(argv[1]);
    if (!check) {
        /* Invalid HTDOCS directory */
        dprint("httpd:: Invalid directory to serve from, quitting!\n");
        return 1;
    } else {
        closedir(check);
    }
    
    return serve(argv[1]);
}

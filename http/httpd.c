#include <sys/types.h>
#include <sys/stat.h>
#include <tcp.h>
#include <dirent.h>


void
send_initial_response(int code)
{
    tcp_write("HTTP/1.0 ", 8);
    switch (code) {
        case 404:
            tcp_write("404 Not Found\r\n", 15);
            break;
        default:
            tcp_write("200 OK\r\n", 8);
            break;
    }
    
    /* FIXME: write date and time */
    tcp_write("Server: CN Practical '08/09 HTTP Server (Minix 3.1)\r\n", 56);   
}

void
handle_get(char* htdocs)
{
    int i = 0;
    char path[256];
    char* ptr = path;
    char* fPath;
    FILE* f;
    
    unsigned char* contents;
    struct stat buf;
    
    do {
        tcp_read(ptr, 1);
        ptr++; i++;
    } while (!strncmp(ptr, " ", 1) && (i < 256));
    
    fPath = (char*)calloc(strlen(htdocs) + i + 1, sizeof(char));
    memcpy(fPath, htdocs, strlen(htdocs));
    memcpy(fPath + strlen(htdocs), path, i);
    
    if (stat(fPath, &buf) != 0) {
        /* Cannot access file because it does not exist? */
        send_initial_response(404);
        tcp_write("\r\n", 2);
        return;
    }
    
    contents = (unsigned char*)calloc(buf.st_size, sizeof(unsigned char));    
    /* FIXME: Get more information from buf, such atime and mtime */
    
    f = fopen(fPath, "r+");
    fread(contents, sizeof(unsigned char), buf.st_size, f);
    fclose(f);
    
    /* Send response */
    send_initial_response(200);
    /* FIXME: Output other headers */
    tcp_write("\r\n", 2);
    tcp_write((char*)contents, buf.st_size);
    
    free(fPath);
    free(contents);
}

int
serve(char* htdocs)
{
    int con;
    char method[4];

    if (!(con = tcp_socket())) {
        dprint("Could not initialize tcp_socket, quitting!\n");
        return 1;
    }
    
    if (!tcp_listen(80, (ipaddr_t*)my_ipaddr)) {
        dprint("Could not listen on port 80, quitting!\n");
        return 1;
    }
    
    /* First, let's find out which HTTP method is requested */
    tcp_read(method, 4);
    
    switch (method[0]) {
        case 'G':
            if (!strcmp(method, "GET ")) {
                handle_get(htdocs);
                tcp_close();
                return 0;
            } else {
                dprint("Invalid HTTP method, quitting!\n");
                tcp_close();
                return 1;
            }
        default:
            /* Other HTTP methods not supported */
            dprint("Method %s not supported, quitting!\n", method);
            tcp_close();
            return 1;
    }
}

int
main(int argc, char** argv)
{
    DIR* check;

    if (argc != 2) {
        /* Invalid arguments passes */
        dprint("Usage: httpd <directory>\n");
        return 1;
    }
    
    check = opendir(argv[1]);
    if (!check) {
        /* Invalid HTDOCS directory */
        dprint("Invalid directory to serve from, quitting!\n");
        return 1;
    } else {
        closedir(check);
    }
    
    return serve(argv[1]);
}


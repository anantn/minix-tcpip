#include <http.h>

int
my_itoa(int n, char* s)
{
    int i, c, j, k;
    
    i = 0;    
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
        

    for (j = 0, k = i - 1; j < k; j++, k--) {
        c = s[j];
        s[j] = s[k];
        s[k] = c;
    }
    s[i] = '\0';

    return i;
}

int
tcp_reliable_read(char* buf, int len)
{
    int i;
    int read = 0;
    
    while (read < len) {
        i = tcp_read(buf + read, len - read);
        if (i == EOF)
            break;
        else
            read += i;
    }
    
    return read;
}

void
chomp(int print)
{
    char c, p;
    p = '\0';
    while (tcp_read(&c, 1)) {
        if (print)
            printf("%c", c);
        if (p == '\r' && c == '\n')
            break;
        p = c;
    }
}

void
read_flush()
{
    char buf;
    int i, state;
    
    i = 0; state = 0;
    while (state != 4) {
        i++;
        if (tcp_read(&buf, 1) != 1) {
            dprint("http:: Client not responding! Abort...\n");
            exit(0);
        }
        switch (buf) {
            case '\r':
                if (state == 0)
                    state = 1;
                else if (state == 2)
                    state = 3;
                else
                    state = 0;
                break;
            case '\n':
                if (state == 1)
                    state = 2;
                else if (state == 3)
                    state = 4;
                else 
                    state = 0;
                break;
            default:
                state = 0;
                break;
        }
    }
}

int
noprint(char* arg, ...)
{
    return 1;
}

/* vim: set ts=4 sw=4 expandtab tw=78: */


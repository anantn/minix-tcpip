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
    int i;
    char buf[4];
    
    i = 0;
    while (tcp_reliable_read(buf, 1)) {
		i++;
        if (buf[3] == '\r' && buf[2] == '\n' &&
            buf[1] == '\r' && buf[0] == '\n') {
		    dprint("http:: Flush DONE, read %d bytes more\n", i);
            break;
	    }
	    buf[3] = buf[2];
		buf[2] = buf[1];
		buf[1] = buf[0];
	}
}

int
noprint(char* arg, ...)
{
    return 1;
}

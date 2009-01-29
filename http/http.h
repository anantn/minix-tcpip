#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <tcp.h>

int noprint(char*a, ...);
int my_itoa(int n, char* s);
int tcp_reliable_read(char* buf, int len);
void read_flush();
void chomp(int print);

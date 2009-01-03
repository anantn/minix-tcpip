#include <stdio.h>
#include "inet.h"
#include "ip.h"

#define MINIXT1_1 "130.37.30.116"
#define MINIXT1_2 "130.37.30.117"

int main(void) {
  int len;
  len = ip_send(inet_addr(MINIXT1_2),IP_PROTO_UDP,2,"foo bar",8);
  printf("len=%d\n",len);
}


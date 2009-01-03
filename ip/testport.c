/*
	testport.c

	Test udp_port().
	Door W.C.A. Wijngaards, 1996

*/

#include <stdio.h>
#include <stdlib.h>
#include <udp.h>

/*
	als het goed is:
	port_t udp_port();
	(port_t is unsigned short)
*/

main()
{
	int i;
	port_t p;
	printf("Testing udp_port\n");
	printf("sizeof(port_t) = %d (should be 2)\n", sizeof(port_t));
	if(sizeof(port_t) != 2)
		printf("WRONG SIZE\n");
	
	for(i=0; i<65550; i++)
	{
		p = udp_port();
		if((i % 1000) == 0)
			printf("Trying %d, port %d\n", i, p);
	}
}

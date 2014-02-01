#include <stdio.h>

#include "ircd.h"

int main(int argc, char *argv[])
{
	u_strop_state st;
	char *s;

	U_STROP_SPLIT(&st, argv[1], " ", &s)
		printf("%s:", s);
	printf("\n");
}

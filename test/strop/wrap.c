#include <stdio.h>

#include "ircd.h"

int main(int argc, char *argv[])
{
	u_strop_wrap w;
	char *s;
	int i;

	u_strop_wrap_start(&w, 20);
	for (i=1; i<=argc; i++) {
		char *word = i < argc ? argv[i] : NULL;
		while (s = u_strop_wrap_word(&w, word))
			printf("... %s\n", s);
	}

	return 0;
}

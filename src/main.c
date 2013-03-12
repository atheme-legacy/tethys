#include "ircd.h"

struct u_io base_io;

short opt_port = 6667;

usage(argv0, code)
{
	printf("Usage: %s [OPTIONS]\n", argv0);
	printf("Options:\n");
	printf("  -h         Print this help and exit\n");
	printf("  -p PORT    The port to listen on for connections\n");
	exit(code);
}

int main(argc, argv)
int argc;
char *argv[];
{
	int c;

	while ((c = getopt(argc, argv, "hp:")) != -1) {
		switch(c) {
		case 'h':
			usage(argv[0], 0);
			break;
		case 'p':
			opt_port = atoi(optarg);
			break;
		case '?':
			usage(argv[0], 1);
			break;
		}
	}

	u_io_init(&base_io);

	if (!u_conn_origin_create(&base_io, INADDR_ANY, 6667, u_toplev_origin_cb)) {
		printf("Could not create connection origin. Bailing\n");
		return 1;
	}

	printf("Entering IO loop\n");

	u_io_poll(&base_io);

	printf("IO loop died. Bye bye!\n");

	return 0;
}

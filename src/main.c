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

#define INIT(fn) if ((err = (fn)()) < 0) return err
#define COMMAND_DEF(cmds) extern struct u_cmd cmds[]
#define COMMAND(cmds) if ((err = u_cmds_reg(cmds)) < 0) return err
COMMAND_DEF(c_ureg);
int init()
{
	int err;

	INIT(init_util);
	INIT(init_user);
	COMMAND(c_ureg);

	return 0;
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

	if (init() < 0) {
		u_log("Initialization failed\n");
		return 1;
	}

	u_io_init(&base_io);

	if (!u_conn_origin_create(&base_io, INADDR_ANY, 6667, u_toplev_origin_cb)) {
		u_log("Could not create connection origin. Bailing\n");
		return 1;
	}

	u_debug("Entering IO loop\n");

	u_io_poll(&base_io);

	u_debug("IO loop died. Bye bye!\n");

	return 0;
}

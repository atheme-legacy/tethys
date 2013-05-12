/* ircd-micro, main.c -- entry point
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

u_io base_io;
u_ts_t started;

short opt_port = 6667;

int usage(argv0, code) char *argv0;
{
	printf("Usage: %s [OPTIONS]\n", argv0);
	printf("Options:\n");
	printf("  -v         Be verbose. Supply multiple times for more verbosity.\n");
	printf("  -h         Print this help and exit\n");
	printf("  -p PORT    The port to listen on for connections\n");
	exit(code);
}

#define INIT(fn) if ((err = (fn)()) < 0) return err
#define COMMAND_DEF(cmds) extern u_cmd cmds[]
#define COMMAND(cmds) if ((err = u_cmds_reg(cmds)) < 0) return err
COMMAND_DEF(c_reg);
COMMAND_DEF(c_user);
COMMAND_DEF(c_server);
int init()
{
	int err;
	FILE *f;
	struct timeval now;

	u_log(LG_DEBUG, "Seeding random number generator...");
	gettimeofday(&now, NULL);
	srand(now.tv_usec);

	signal(SIGPIPE, SIG_IGN);

	INIT(init_util);
	INIT(init_dns);
	INIT(init_upgrade);
	INIT(init_conf);
	INIT(init_auth);
	INIT(init_user);
	INIT(init_cmd);
	INIT(init_server);
	INIT(init_chan);
	INIT(init_sendto);
	COMMAND(c_reg);
	COMMAND(c_user);
	COMMAND(c_server);

	u_io_init(&base_io);
	u_dns_use_io(&base_io);

	f = fopen("etc/micro.conf", "r");
	if (f == NULL) {
		u_log(LG_SEVERE, "Could not find etc/micro.conf!");
		return -1;
	}
	u_conf_read(f);
	fclose(f);

	return 0;
}

extern char *optarg;

int main(argc, argv) char *argv[];
{
	int c;

	u_log(LG_INFO, "%s starting...", PACKAGE_FULLNAME);
	started = time(NULL);

	while ((c = getopt(argc, argv, "vhp:")) != -1) {
		switch(c) {
		case 'v':
			if (u_log_level < LG_FINE)
				u_log_level++;
			break;
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
		u_log(LG_ERROR, "Initialization failed");
		return 1;
	}

	if (!u_conn_origin_create(&base_io, INADDR_ANY, opt_port)) {
		u_log(LG_SEVERE, "Could not create listener on port %d. Bailing",
		      opt_port);
		return 1;
	}

	u_log(LG_INFO, "Entering IO loop");

	u_io_poll(&base_io);

	u_log(LG_VERBOSE, "IO loop died. Bye bye!");

	return 0;
}

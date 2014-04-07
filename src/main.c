/* Tethys, main.c -- entry point
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

struct timeval NOW;

void sync_time(void)
{
	gettimeofday(&NOW, NULL);
}

mowgli_eventloop_t *base_ev;
mowgli_dns_t *base_dns;
u_ts_t started;
char startedstr[256];

ushort opt_port = 0;

char *main_argv0;

int usage(char *argv0, int code)
{
	printf("Usage: %s [OPTIONS]\n", argv0);
	printf("Options:\n");
	printf("  -v         Be verbose. Supply multiple times for more verbosity.\n");
	printf("  -h         Print this help and exit\n");
	exit(code);
}

#define INIT(fn) if ((err = (fn)()) < 0) return err
int init(void)
{
	int err;

	sync_time();

	u_log(LG_DEBUG, "Seeding random number generator...");
	srand(NOW.tv_usec);

	signal(SIGPIPE, SIG_IGN);

	base_ev = mowgli_eventloop_create();
	base_dns = mowgli_dns_create(base_ev, MOWGLI_DNS_TYPE_ASYNC);

	INIT(init_upgrade);
	INIT(init_util);
	INIT(init_module);
	INIT(init_hook);
	INIT(init_conf);
	INIT(init_conn);
	INIT(init_auth);
	INIT(init_server);
	INIT(init_user);
	INIT(init_cmd);
	INIT(init_chan);
	INIT(init_sendto);
	INIT(init_link);

	u_module_load_directory("modules/core");

	/* LINK TODO: add ping timer */

	if (opt_port != 0 && u_link_origin_create(base_ev, opt_port) < 0)
		return -1;

	if (!u_conf_read("etc/tethys.conf")) {
		u_log(LG_SEVERE, "Could not find etc/tethys.conf!");
		return -1;
	}

	/* Upgrade */
	if (upgrade_json) {
		INIT(restore_server);
		INIT(restore_user);
		INIT(restore_chan);
		finish_upgrade();
		u_server_flush_inputs();
		u_user_flush_inputs();
	}
	/* Any upgrade is now complete. */

	return 0;
}

extern char *optarg;
int main(int argc, char **argv)
{
	int c, i;

	main_argv0 = argv[0];

	gettimeofday(&NOW, NULL);
	started = NOW.tv_sec;
	strftime(startedstr, 256, "%a %b %d at %T UTC", gmtime((time_t*) &started));

	u_log(LG_INFO, "%s starting...", PACKAGE_FULLNAME);

	while ((c = getopt(argc, argv, "vhp:U:")) != -1) {
		switch(c) {
		case 'v':
			if (u_log_level < LG_FINE)
				u_log_level++;
			break;
		case 'h':
			usage(argv[0], 0);
			break;
		case 'p':
			u_log(LG_WARN, "Use of -p is deprecated. Please use the"
			      " config file to specify listeners");
			i = atoi(optarg);
			if (i < 1 || i > 65535) {
				u_log(LG_WARN, "-p port invalid");
				break;
			}
			opt_port = (ushort) i;
			break;
		case 'U':
			opt_upgrade = optarg;
			break;
		case '?':
			usage(argv[0], 1);
			break;
		}
	}

	if (init() < 0) {
		u_log(LG_ERROR, "Initialization failed");
		abort_upgrade();
		return 1;
	}

	u_log(LG_INFO, "Entering IO loop");

	u_conn_run(base_ev);

	u_log(LG_VERBOSE, "IO loop died. Bye bye!");

	return 0;
}

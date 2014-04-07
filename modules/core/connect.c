/* Tethys, core/connect -- CONNECT command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void notice(u_user *u, const char *fmt, ...)
{
	char buf[512];
	va_list va;

	va_start(va, fmt);
	vsnf(FMT_USER, buf, 512, fmt, va);
	va_end(va);

	if (u == NULL) {
		/* ?? */
		u_log(LG_INFO, "notice: %s", buf);
	} else {
		u_link_f(u->link, ":%S NOTICE %U :%s", &me, u, buf);
	}
}

static int c_lo_connect(u_sourceinfo *si, u_msg *msg)
{
	/* TODO: remote version */

	u_link_block *block;
	if (! (block = u_find_link(msg->argv[0]))) {
		notice(si->u, "connect: host %s not in config", msg->argv[0]);
		return 0;
	}

	int port = block->port;
	if (msg->argc > 1) {
		port = atoi(msg->argv[1]);
		if (port < 1 || port > 65535) {
			port = block->port;
			notice(si->u, "connect: %s: invalid port number; "
			       "using %d in config", msg->argv[1], port);
		}
	}

	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	if (! u_pton(block->host, (struct sockaddr*) &addr, &addrlen)) {
		notice(si->u, "connect: couldn't make host %s into an address", block->host);
		return 0;
	}
	switch (addr.ss_family) {
		case AF_INET:
			((struct sockaddr_in*) &addr)->sin_port = htons((ushort) port);
			break;
		case AF_INET6:
			((struct sockaddr_in6*) &addr)->sin6_port = htons((ushort) port);
			break;
	}
	u_link_connect(base_ev, block, (struct sockaddr*) &addr, addrlen);

	return 0;
}

static u_cmd connect_cmdtab[] = {
	{ "CONNECT", SRC_LOCAL_OPER, c_lo_connect, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/connect", "Alex Iadicicco", "CONNECT command",
	NULL, NULL, connect_cmdtab);

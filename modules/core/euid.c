/* Tethys, core/euid -- EUID command
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_s_euid(u_sourceinfo *si, u_msg *msg)
{
	u_user *u, *tu;
	u_modes m;

	if (!si->s) {
		u_log(LG_SEVERE, "EUID from unknown server %s", msg->srcstr);
		return 0;
	}

	tu = u_user_by_nick(msg->argv[0]);
	if (tu != NULL && !u_user_try_override(tu)) {
		/* TODO: perform nick collision */
		u_log(LG_SEVERE, "Need to perform nick collision!");
	}

	u = u_user_create_remote(si->s, msg->argv[7]);

	u_user_set_nick(u, msg->argv[0], atoi(msg->argv[2]));
	u_strlcpy(u->ident, msg->argv[4], MAXIDENT+1);
	u_strlcpy(u->host, msg->argv[5], MAXHOST+1);
	u_strlcpy(u->ip, msg->argv[6], INET_ADDRSTRLEN);
	u_strlcpy(u->gecos, msg->argv[msg->argc - 1], MAXGECOS+1);
	u_strlcpy(u->realhost, msg->argv[8], MAXHOST+1);
	if (msg->argv[9][0] != '*')
		u_strlcpy(u->acct, msg->argv[9], MAXACCOUNT+1);

	/* user modes */
	m.ctx = &umodes;
	m.stacker = NULL;
	m.setter = si;
	m.target = u;
	m.access = NULL;
	m.flags = MODE_FORCE_ALL;
	u_mode_process(&m, 1, msg->argv + 3);

	u_log(LG_VERBOSE, "%U has user mode %s", u, u_user_modes(u));

	msg->propagate = CMD_DO_BROADCAST;

	return 0;
}

static u_cmd euid_cmdtab[] = {
	{ "EUID", SRC_SERVER, c_s_euid, 11, CMD_PROP_BROADCAST },
	{ }
};

TETHYS_MODULE_V1(
	"core/euid", "Alex Iadicicco", "EUID command",
	NULL, NULL, euid_cmdtab);

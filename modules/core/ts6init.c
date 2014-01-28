/* Tethys, core/ts6init -- Commands for TS6 connection setup
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_us_pass(u_sourceinfo *si, u_msg *msg)
{
	if (!streq(msg->argv[1], "TS") || !streq(msg->argv[2], "6")) {
		u_conn_fatal(si->source, "Invalid TS version");
		return 0;
	}

	if (si->source->pass != NULL)
		free(si->source->pass);
	si->source->pass = strdup(msg->argv[0]);
}

static int c_us_capab(u_sourceinfo *si, u_msg *msg)
{
	u_server_add_capabs(si->s, msg->argv[0]);
	return 0;
}

static int c_us_server(u_sourceinfo *si, u_msg *msg)
{
	u_strlcpy(si->s->name, msg->argv[0], MAXSERVNAME+1);
	u_strlcpy(si->s->desc, msg->argv[2], MAXSERVDESC+1);

	/* attempt server registration */
	u_link *link;
	uint capab_need = CAPAB_QS | CAPAB_EX | CAPAB_IE
	                | CAPAB_EUID | CAPAB_ENCAP;

	if ((si->s->capab & capab_need) != capab_need) {
		u_conn_fatal(si->source, "Don't have all needed CAPABs!");
		return 0;
	}

	if (!(link = u_find_link(si->source))) {
		u_conn_fatal(si->source, "No link{} blocks for your host");
		return 0;
	}

	si->source->flags |= U_CONN_REGISTERED;

	u_sendto_servers(si->source, ":%S SID %s %d %s :%s", &me,
	                 si->s->name, si->s->hops, si->s->sid, si->s->desc);

	u_server_burst_1(si->s, link);
	u_server_burst_2(si->s, link);

	return 0;
}

static int c_ls_svinfo(u_sourceinfo *si, u_msg *msg)
{
	int tsdelta;

	/* I suppose this could be sent at any time, but any correct TS6
	   implementation will send it as the first line of a netburst,
	   and never any other time. */

	if (atoi(msg->argv[0]) < 6) {
		u_conn_fatal(si->source, "Max TS version less than 6!");
		return 0;
	}

	tsdelta = atoi(msg->argv[3]) - (int)NOW.tv_sec;
	if (tsdelta < 0)
		tsdelta = -tsdelta;

	if (tsdelta > 10)
		u_log(LG_WARN, "%S has TS delta of %d", si->s, tsdelta);

	if (tsdelta > 60) {
		u_log(LG_ERROR, "%S has excessive TS delta, killing", si->s);
		u_conn_fatal(si->source, "Excessive TS delta");
		return 0;
	}

	return 0;
}

static u_cmd ts6init_cmdtab[] = {
	{ "PASS",    SRC_UNREGISTERED_SERVER,  c_us_pass,   4 },
	{ "CAPAB",   SRC_UNREGISTERED_SERVER,  c_us_capab,  1 },
	{ "SERVER",  SRC_UNREGISTERED_SERVER,  c_us_server, 3 },
	{ "SVINFO",  SRC_LOCAL_SERVER,         c_ls_svinfo, 4 },
	{ }
};

static int init_capab(u_module *m)
{
	u_cmds_reg(ts6init_cmdtab);
}

TETHYS_MODULE_V1(
	"core/ts6init", "Alex Iadicicco",
	"Initial TS6 commands, PASS CAPAB SERVER and SVINFO",
	init_capab, NULL);

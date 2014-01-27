/* Tethys, core/pass.c -- PASS command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_first_pass(u_sourceinfo *si, u_msg *msg)
{
	if (msg->argc == 1)
		return u_repeat_as_user(si, msg);

	if (msg->argc < 4) {
		u_conn_num(si->source, ERR_NEEDMOREPARAMS, "PASS");
		return 0;
	}

	if (!streq(msg->argv[1], "TS") || !streq(msg->argv[2], "6")) {
		u_conn_fatal(si->source, "Invalid TS version");
		return 0;
	}

	if (!is_valid_sid(msg->argv[3])) {
		u_conn_fatal(si->source, "Invalid SID");
		return;
	}

	return u_repeat_as_server(si, msg, msg->argv[3]);
}

static int c_uu_pass(u_sourceinfo *si, u_msg *msg)
{
	u_conn_f(si->source, ":%S NOTICE * :WARNING -- PASS unimplemented", &me);
	return 0;
}

static int c_us_pass(u_sourceinfo *si, u_msg *msg)
{
	u_conn_f(si->source, ":%S NOTICE * :WARNING -- PASS unimplemented", &me);
	return 0;
}

static u_cmd pass_cmdtab[] = {
	{ "PASS", SRC_FIRST,                 c_first_pass, 0 },
	{ "PASS", SRC_UNREGISTERED_USER,     c_uu_pass, 1 },
	{ "PASS", SRC_UNREGISTERED_SERVER,   c_us_pass, 4 },
	{ }
};

static int init_pass(u_module *m)
{
	u_cmds_reg(pass_cmdtab);
}

TETHYS_MODULE_V1(
	"core/pass", "Alex Iadicicco", "PASS command",
	init_pass, NULL);
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
		u_link_num(si->source, ERR_NEEDMOREPARAMS, "PASS");
		return 0;
	}

	if (!is_valid_sid(msg->argv[3])) {
		u_link_fatal(si->source, "Invalid SID");
		return 0;
	}

	return u_repeat_as_server(si, msg, msg->argv[3]);
}

static int c_uu_pass(u_sourceinfo *si, u_msg *msg)
{
	if (si->source->pass != NULL)
		free(si->source->pass);
	si->source->pass = strdup(msg->argv[0]);

	return 0;
}

static u_cmd pass_cmdtab[] = {
	{ "PASS", SRC_FIRST,                 c_first_pass, 0 },
	{ "PASS", SRC_UNREGISTERED_USER,     c_uu_pass, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/pass", "Alex Iadicicco", "PASS command",
	NULL, NULL, pass_cmdtab);

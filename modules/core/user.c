/* Tethys, core/user.c -- USER command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_uu_user(u_sourceinfo *si, u_msg *msg)
{
	char buf[MAXIDENT+1];

	u_strlcpy(buf, msg->argv[0], MAXIDENT+1); /* truncate */

	if (!is_valid_ident(buf))
		return u_conn_num(si->source, ERR_GENERIC, "Invalid username");

	u_strlcpy(si->u->ident, buf, MAXIDENT+1);
	u_strlcpy(si->u->gecos, msg->argv[3], MAXGECOS+1);

	u_user_try_register(si->u);

	return 0;
}

static u_cmd user_cmdtab[] = {
	{ "USER", SRC_FIRST,              u_repeat_as_user, 0 },
	{ "USER", SRC_UNREGISTERED_USER,  c_uu_user, 4 },
	{ }
};

TETHYS_MODULE_V1(
	"core/user", "Alex Iadicicco", "USER command",
	NULL, NULL, user_cmdtab);

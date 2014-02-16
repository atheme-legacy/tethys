/* Tethys, core/kill -- KILL command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_a_kill(u_sourceinfo *si, u_msg *msg)
{
	u_user *tu;
	char *reason = msg->argv[1];
	char buf[512];

	if (!(tu = u_user_by_ref(si->source, msg->argv[0])))
		return u_src_num(si, ERR_NOSUCHNICK, msg->argv[0]);

	if (SRC_IS_LOCAL_USER(si)) {
		if (reason == NULL)
			reason = "<No reason given>";
		snprintf(buf, 512, " :%s (%s)", si->name, reason);
	} else {
		/* reason is actually technically path in this case */
		buf[0] = '\0';
		if (reason != NULL)
			snprintf(buf, 512, " :%s", reason);
	}

	u_sendto_visible(tu, ST_USERS, ":%H QUIT :Killed (%s)", tu, buf + 2);
	u_sendto_servers(si->source, ":%I KILL %U%s", si, tu, buf);

	if (IS_LOCAL_USER(tu))
		u_link_f(tu->link, ":%H QUIT :Killed (%s)", tu, buf + 2);
	u_user_destroy(tu);

	return 0;
}

static u_cmd kill_cmdtab[] = {
	{ "KILL", SRC_S2S | SRC_LOCAL_OPER, c_a_kill, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/kill", "Alex Iadicicco", "KILL command",
	NULL, NULL, kill_cmdtab);

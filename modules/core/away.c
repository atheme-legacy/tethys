/* Tethys, core/away -- AWAY command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_u_away(u_sourceinfo *si, u_msg *msg)
{
	char *r = msg->argv[0];

	if (!r || !*r) {
		si->u->away[0] = '\0';
		if (IS_LOCAL_USER(si->u))
			u_user_num(si->u, RPL_UNAWAY);
		u_sendto_servers(si->source, ":%I AWAY", si);
	} else {
		u_strlcpy(si->u->away, r, MAXAWAY);
		if (IS_LOCAL_USER(si->u))
			u_user_num(si->u, RPL_NOWAWAY);
		u_sendto_servers(si->source, ":%I AWAY :%s", si, r);
	}

	return 0;
}

static u_cmd away_cmdtab[] = {
	{ "AWAY", SRC_USER, c_u_away, 0 },
	{ },
};

TETHYS_MODULE_V1(
	"core/away", "Alex Iadicicco", "AWAY command",
	NULL, NULL, away_cmdtab);

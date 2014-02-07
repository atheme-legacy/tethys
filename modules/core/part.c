/* Tethys, core/part -- PART command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_u_part(u_sourceinfo *si, u_msg *msg)
{
	char *q, *s, *p, chans[512], reason[512];
	u_chan *c;
	u_chanuser *cu;
	bool local = SRC_IS_LOCAL_USER(si);

	reason[0] = '\0';
	if (msg->argv[1])
		snprintf(reason, 512, " :%s", msg->argv[1]);

	q = chans;

	p = msg->argv[0];
	while ((s = cut(&p, ",")) != NULL) {
		u_log(LG_FINE, "%s PART %s$%s", si->u->nick, s, p);

		cu = NULL;

		if (!(c = u_chan_get(s))) {
			if (local)
				u_user_num(si->u, ERR_NOSUCHCHANNEL, s);
		} else if (!(cu = u_chan_user_find(c, si->u))) {
			if (local)
				u_user_num(si->u, ERR_NOTONCHANNEL, c);
		} else {
			u_sendto_chan(c, NULL, ST_USERS, ":%H PART %C%s",
			              si->u, c, reason);
		}

		if (cu || !local)
			q += sprintf(q, "%s%s", q==chans?"":",", s);

		if (cu)
			u_chan_user_del(cu);
	}

	if (q != chans) {
		u_log(LG_DEBUG, "%U parted from %s%s", si->u, chans, reason);
		u_sendto_servers(si->source, ":%U PART %s%s",
		           si->u, chans, reason);
	}

	return 0;
}

static u_cmd part_cmdtab[] = {
	{ "PART", SRC_USER, c_u_part, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/part", "Alex Iadicicco", "PART command",
	NULL, NULL, part_cmdtab);

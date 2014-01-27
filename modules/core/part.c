/* Tethys, core/part -- PART command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_lu_part(u_sourceinfo *si, u_msg *msg)
{
	char *q, *s, *p, chans[512], reason[512];
	u_chan *c;
	u_chanuser *cu;

	reason[0] = '\0';
	if (msg->argv[1])
		snprintf(reason, 512, " :%s", msg->argv[1]);

	q = chans;

	p = msg->argv[0];
	while ((s = cut(&p, ",")) != NULL) {
		u_log(LG_FINE, "%s PART %s$%s", si->u->nick, s, p);

		if (!(c = u_chan_get(s))) {
			u_user_num(si->u, ERR_NOSUCHCHANNEL, s);
			continue;
		}

		if (!(cu = u_chan_user_find(c, si->u))) {
			u_user_num(si->u, ERR_NOTONCHANNEL, c);
			continue;
		}

		u_sendto_chan(c, NULL, ST_USERS, ":%H PART %C%s", si->u, c, reason);
		u_chan_user_del(cu);

		q += sprintf(q, "%s%s", q==chans?"":",", c->name);

		if (c->members->size == 0 && !(c->flags & CHAN_PERMANENT)) {
			u_log(LG_DEBUG, "Dropping channel %C", c);
			u_chan_drop(c);
		}
	}

	if (q != chans) {
		u_log(LG_DEBUG, "%U parted from %s%s", si->u, chans, reason);
		u_sendto_servers(si->link, ":%U PART %s%s",
		           si->u, chans, reason);
	}

	return 0;
}

static u_cmd part_cmdtab[] = {
	{ "PART", SRC_LOCAL_USER, c_lu_part, 1 },
	{ }
};

static int init_part(u_module *m)
{
	u_cmds_reg(part_cmdtab);
}

TETHYS_MODULE_V1(
	"core/part", "Alex Iadicicco", "PART command",
	init_part, NULL);

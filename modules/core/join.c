/* Tethys, core/join -- JOIN and SJOIN commands
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int try_local_join_chan(u_sourceinfo *si, char *chan, char *key)
{
	u_chan *c, *fwd;
	u_chanuser *cu;
	bool created;
	int num;
	char *modes;

	/* verify entry */

	if (!(c = u_chan_get_or_create(chan)))
		return u_user_num(si->u, ERR_NOSUCHCHANNEL, chan);
	created = c->ts == NOW.tv_sec; /* is this ok? */

	if ((cu = u_chan_user_find(c, si->u)) != NULL)
		return 0;

	if ((num = u_entry_blocked(c, si->u, key)) != 0) {
		fwd = u_find_forward(c, si->u, key);
		if (fwd == NULL || u_chan_user_find(fwd, si->u))
			return u_user_num(si->u, num, c);
		c = fwd;
	}

	/* perform join */

	cu = u_chan_user_add(c, si->u);
	u_del_invite(c, si->u);
	if (created)
		cu->flags |= CU_PFX_OP;

	/* send messages */

	u_sendto_chan(c, NULL, ST_USERS, ":%H JOIN %C", si->u, c);

	modes = u_chan_modes(c, 1);
	if (created) {
		u_log(LG_VERBOSE, "Channel %C %s created by %U", c, modes, si->u);
		u_conn_f(si->link, ":%S MODE %C %s", &me, c, modes);
	}

	if (!(c->flags & CHAN_LOCAL)) {
		if (c->members->size == 1) {
			u_sendto_servers(NULL, ":%S SJOIN %u %C %s :%s%U",
			           &me, c->ts, c, modes,
			           (cu->flags & CU_PFX_OP) ? "@" : "", si->u);
		} else {
			u_sendto_servers(NULL, ":%U JOIN %u %C +",
			           si->u, c->ts, c);
		}
	}

	/* Credit them */
	u_ratelimit_who_credit(si->u);

	u_chan_send_topic(c, si->u);
	u_chan_send_names(c, si->u);

	return 0;
}

static int c_lu_join(u_sourceinfo *si, u_msg *msg)
{
	char *keys[128], **keys_p;
	char *s, *p;
	int i;

	p = msg->argv[1];
	for (i=0; i<128; i++)
		keys[i] = cut(&p, ",");
	keys_p = keys;

	p = msg->argv[0];
	while ((s = cut(&p, ",")) != NULL)
		try_local_join_chan(si, s, *keys_p++);

	return 0;
}

static u_chanuser *do_remote_join_chan(u_user *u, u_chan *c)
{
	u_chanuser *cu;

	if (IS_LOCAL_USER(u))
		u_log(LG_WARN, "do_remote_join_chan on local user %U", u);

	if ((cu = u_chan_user_find(c, u))) {
		u_log(LG_WARN, "Already have chanuser %U/%C; ignoring", u, c);
	} else if (!(cu = u_chan_user_add(c, u))) {
		u_log(LG_ERROR, "Could not create chanuser %U/%C", u, c);
		return NULL;
	}

	u_sendto_chan(c, NULL, ST_USERS, ":%H JOIN :%C", u, c);

	return cu;
}

static int c_ru_join(u_sourceinfo *si, u_msg *msg)
{
	u_chan *c;

	/* TODO: react to some of these conditions */

	if (!si->u)
		return 0;
	if (!(c = u_chan_get(msg->argv[1])))
		return 0;
	if (c->flags & CHAN_LOCAL)
		return 0;

	/* TODO: check TS */

	/* perform join, send local JOIN */
	do_remote_join_chan(si->u, c);
	msg->propagate = CMD_DO_BROADCAST;

	return 0;
}

static u_cmd join_cmdtab[] = {
	{ "JOIN",   SRC_LOCAL_USER,   c_lu_join, 1, 0, U_RATELIMIT_MID },
	{ "JOIN",   SRC_REMOTE_USER,  c_ru_join, 3, CMD_PROP_BROADCAST },
	{ }
};

TETHYS_MODULE_V1(
	"core/join", "Alex Iadicicco", "JOIN and SJOIN commands",
	NULL, NULL, join_cmdtab);

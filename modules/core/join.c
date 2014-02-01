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

static int c_s_sjoin(u_sourceinfo *si, u_msg *msg)
{
	u_user *u;
	u_chan *c;
	u_chanuser *cu;
	u_strop_state st;
	char *s;
	char *m, mbuf[16];
	uint flags;

	if (!(c = u_chan_get_or_create(msg->argv[1])))
		return u_log(LG_ERROR, "Could not get/create %s for SJOIN",
		             msg->argv[1]);
	if (c->flags & CHAN_LOCAL)
		return u_log(LG_ERROR, "Cannot SJOIN to a local channel");

	/* TODO: check TS */

	/* XXX: this method of processing channel modes is not 100% correct */
	u_modes modes;
	modes.setter = si;
	modes.target = c;
	modes.perms = &me;
	modes.flags = 0;
	u_mode_process(&modes, cmodes, msg->argc - 3, msg->argv + 2);
	if (modes.u.buf[0] || modes.u.data[0]) {
		u_sendto_chan(c, NULL, ST_USERS, ":%I MODE %C %s%s",
		              si, c, modes.u.buf, modes.u.data);
	}

	U_STROP_SPLIT(&st, msg->argv[msg->argc - 1], " ", &s) {
		/* TODO: verify user is behind si->source! */

		/* parse prefix into mode chars and flags */
		m = mbuf;
		for (flags = 0; !isdigit(*s); s++) {
			switch (*s) {
			case '@':
				*m++ = 'o';
				flags |= CU_PFX_OP;
				break;
			case '+':
				*m++ = 'v';
				flags |= CU_PFX_VOICE;
				break;
			default:
				u_log(LG_WARN, "SJOIN: %S using unknown prefix %c",
				      si->s, *s);
			}
		}
		*m++ = '\0';

		if (!(u = u_user_by_uid(s))) {
			u_log(LG_ERROR, "%S tried to SJOIN nonexistent %s!",
			      si->s, s);
			continue;
		}

		/* perform join, send local JOIN */
		if (!(cu = do_remote_join_chan(u, c))) {
			u_log(LG_ERROR, "SJOIN: do_remote_join_chan failed!!");
			continue;
		}
		cu->flags |= flags;

		/* send local MODE message, if relevant */
		if (mbuf[0] && !mbuf[1]) {
			u_sendto_chan(c, NULL, ST_USERS,
			              ":%S MODE %C +%s %s",
			              si->s, c, mbuf, u->nick);
		} else if (mbuf[0] && mbuf[1]) {
			u_sendto_chan(c, NULL, ST_USERS,
			              ":%S MODE %C +%s %s %s",
			              si->s, c, mbuf, u->nick, u->nick);
		}
	}

	msg->propagate = CMD_DO_BROADCAST;
	return 0;
}

static u_cmd join_cmdtab[] = {
	{ "JOIN",   SRC_LOCAL_USER,   c_lu_join, 1 },
	{ "JOIN",   SRC_REMOTE_USER,  c_ru_join, 3, CMD_PROP_BROADCAST },
	{ "SJOIN",  SRC_SERVER,       c_s_sjoin, 4, CMD_PROP_BROADCAST },
	{ }
};

TETHYS_MODULE_V1(
	"core/join", "Alex Iadicicco", "JOIN and SJOIN commands",
	NULL, NULL, join_cmdtab);

/* Tethys, core/mode -- MODE and TMODE commands
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int mode_user(u_sourceinfo *si, char *s)
{
	u_modes m;

	if (!s || !*s) {
		u_src_num(si, RPL_UMODEIS, u_user_modes(si->u));
		return 0;
	}

	m.perms = SRC_IS_LOCAL_USER(si) ? NULL : &me;
	m.target = si->u;

	u_mode_process(&m, umodes, 1, &s);

	if ((m.u.buf[0] || m.u.data[0]) && IS_LOCAL_USER(si->u)) {
		u_conn *conn = u_user_conn(si->u);
		u_conn_f(conn, ":%U MODE %U %s%s",
		         si->u, si->u, m.u.buf, m.u.data);
	}

	if (m.s.buf[0] || m.s.data[0]) {
		u_sendto_servers(si->link, ":%U MODE %U %s%s",
		                 si->u, si->u, m.s.buf, m.s.data);
	}

	u_log(LG_VERBOSE, "%U now has user mode %s",
	      si->u, u_user_modes(si->u));

	return 0;
}

static void send_chan_mode_change(u_sourceinfo *si, u_modes *m, u_chan *c)
{
	if (m->u.buf[0] || m->u.data[0]) {
		u_sendto_chan(c, NULL, ST_USERS, ":%I MODE %C %s%s",
		              si, c, m->u.buf, m->u.data);
	}

	if (m->s.buf[0] || m->s.data[0]) {
		u_sendto_servers(si->source, ":%I TMODE %u %C %s%s",
		                 si, c->ts, c, m->s.buf, m->s.data);
	}
}

/* this function is carefully written to handle both local and remote users
   and servers. */
static int c_a_mode(u_sourceinfo *si, u_msg *msg)
{
	char *target = msg->argv[0];
	int parc;
	char **parv;
	u_chan *c;
	u_modes m;
	u_mode_info *info;

	if (!strchr(CHANTYPES, *target)) {
		u_user *tu = u_user_by_nick(target);
		if (tu == NULL) {
			/* legacy chary behavior */
			u_user_num(si->u, ERR_NOSUCHCHANNEL, target);
		} else if (si->u != tu) {
			u_user_num(si->u, ERR_USERSDONTMATCH);
		} else {
			return mode_user(si, msg->argv[1]);
		}
		return 0;
	}

	if (!(c = u_chan_get(target)))
		return u_user_num(si->u, ERR_NOSUCHCHANNEL, target);

	if (SRC_IS_LOCAL_USER(si)) {
		m.perms = u_chan_user_find(c, si->u);

		if (msg->argc == 1) {
			return u_user_num(si->u, RPL_CHANNELMODEIS, c,
					  u_chan_modes(c, !!m.perms));
		}
	} else { /* source is local server or remote user/server */
		m.perms = &me;

		if (msg->argc == 1)
			return 0;
	}

	parc = msg->argc - 1;
	parv = msg->argv + 1;
	if (parc > 5)
		parc = 5;

	m.setter = si;
	m.target = c;
	m.flags = 0;

	u_mode_process(&m, cmodes, parc, parv);

	send_chan_mode_change(si, &m, c);

	if (SRC_IS_LOCAL_USER(si)) {
		for (info=cmodes; info->ch; info++) {
			if (!strchr(m.list, info->ch))
				continue;

			u_chan_send_list(c, si->u, memberp(c, info->data));
		}

		if (m.flags & CM_DENY)
			u_user_num(si->u, ERR_CHANOPRIVSNEEDED, c);
	}

	return 0;
}

static int c_r_tmode(u_sourceinfo *si, u_msg *msg)
{
	char *target = msg->argv[1];
	int parc;
	char **parv;
	u_chan *c;
	u_modes m;

	if (!(c = u_chan_get(target))) {
		return u_log(LG_ERROR, "%G tried to TMODE nonexistent %s",
		             si->source, target);
	}

	/* TODO: check TS */

	parc = msg->argc - 2;
	parv = msg->argv + 2;

	m.setter = si;
	m.target = c;
	m.perms = &me;
	m.flags = 0;

	u_mode_process(&m, cmodes, parc, parv);

	send_chan_mode_change(si, &m, c);
}

static u_cmd mode_cmdtab[] = {
	{ "MODE",  SRC_ANY,  c_a_mode,  1 },
	{ "TMODE", SRC_S2S,  c_r_tmode, 2 },
	{ }
};

TETHYS_MODULE_V1(
	"core/mode", "Alex Iadicicco", "MODE and TMODE commands",
	NULL, NULL, mode_cmdtab);

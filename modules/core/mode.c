/* Tethys, core/mode -- MODE and TMODE commands
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int mode_user(u_sourceinfo *si, char *s)
{
/*
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
*/
}

/* TODO: this is kind of brain-damaged and needs to be redone to use buffers */

static void cmode_stacker_put_external(u_modes *m, int on, char *param)
{
	u_sendto_chan(m->target, NULL, ST_USERS, ":%I MODE %c%c%s%s",
	              m->setter, on ? '+' : '-',
	              m->info->ch, param ? " " : "", param ? param : "");
}

static void cmode_stacker_put_status(u_modes *m, int on, void *tgt)
{
	u_chanuser *cu = tgt;
	u_sendto_chan(m->target, NULL, ST_USERS, ":%I MODE %c%c %U",
	              m->setter, on ? '+' : '-', m->info->ch, cu->u);
}

static void cmode_stacker_put_flag(u_modes *m, int on)
{
	u_sendto_chan(m->target, NULL, ST_USERS, ":%I MODE %c%c",
	              m->setter, on ? '+' : '-', m->info->ch);
}

static void cmode_stacker_put_banlist(u_modes *m, int on, u_chanban *ban)
{
	u_sendto_chan(m->target, NULL, ST_USERS, ":%I MODE %c%c %s",
	              m->setter, on ? '+' : '-', m->info->ch, ban->mask);
}

static u_mode_stacker cmode_stacker = {
	.start          = NULL,
	.put_external   = cmode_stacker_put_external,
	.put_status     = cmode_stacker_put_status,
	.put_flag       = cmode_stacker_put_flag,
	.put_banlist    = cmode_stacker_put_banlist,
	.end            = NULL,
};

/* this function is carefully written to handle both local and remote users
   and servers. */
static int c_a_mode(u_sourceinfo *si, u_msg *msg)
{
	char *target = msg->argv[0];
	int parc;
	char **parv;
	u_chan *c;
	u_modes m;

	if (!strchr(CHANTYPES, *target)) {
		u_user *tu = u_user_by_ref(si->source, target);
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

	m.access = NULL;

	if (SRC_IS_LOCAL_USER(si)) {
		u_chanuser *cu = u_chan_user_find(c, si->u);

		if (msg->argc == 1) {
			return u_user_num(si->u, RPL_CHANNELMODEIS, c,
					  u_chan_modes(c, !!cu));
		}

		if (cu->flags & CU_PFX_OP)
			m.access = cu;

	} else { /* source is local server or remote user/server */
		m.access = &me;

		if (msg->argc == 1)
			return 0;
	}

	parc = msg->argc - 1;
	parv = msg->argv + 1;
	if (parc > 5)
		parc = 5;

	m.ctx = &cmodes;
	m.stacker = &cmode_stacker;
	m.setter = si;
	m.target = c;

	u_mode_process(&m, parc, parv);

	if (SRC_IS_LOCAL_USER(si)) {
		if (m.errors & MODE_ERR_NO_ACCESS)
			u_src_num(si, ERR_CHANOPRIVSNEEDED, c);
		if (m.errors & MODE_ERR_NOT_OPER)
			u_src_num(si, ERR_NOPRIVILEGES);
	}

	return 0;
}

static int c_r_tmode(u_sourceinfo *si, u_msg *msg)
{
/*
	char *target = msg->argv[1];
	int parc;
	char **parv;
	u_chan *c;
	u_modes m;

	if (!(c = u_chan_get(target))) {
		return u_log(LG_ERROR, "%G tried to TMODE nonexistent %s",
		             si->source, target);
	}

	* TODO: check TS *

	parc = msg->argc - 2;
	parv = msg->argv + 2;

	m.setter = si;
	m.target = c;
	m.perms = &me;
	m.flags = 0;

	u_mode_process(&m, cmodes, parc, parv);

	send_chan_mode_change(si, &m, c);
*/
}

static u_cmd mode_cmdtab[] = {
	{ "MODE",  SRC_ANY,  c_a_mode,  1 },
	{ "TMODE", SRC_S2S,  c_r_tmode, 2 },
	{ }
};

TETHYS_MODULE_V1(
	"core/mode", "Alex Iadicicco", "MODE and TMODE commands",
	NULL, NULL, mode_cmdtab);

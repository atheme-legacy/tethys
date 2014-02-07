/* Tethys, core/mode -- MODE and TMODE commands
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int mode_user(u_sourceinfo *si, char *s)
{
	u_modes m;
	u_mode_buf_stack stack;

	if (!s || !*s) {
		u_src_num(si, RPL_UMODEIS, u_user_modes(si->u));
		return 0;
	}

	m.ctx = &umodes;
	m.stacker = &u_mode_buf_stacker;
	m.setter = si;
	m.target = si->u;
	m.access = si;
	m.flags = SRC_IS_LOCAL_USER(si) ? 0 : MODE_FORCE_ALL;
	m.stack = &stack;

	u_mode_process(&m, 1, &s);

	if (stack.on != -1) {
		if (IS_LOCAL_USER(si->u)) {
			u_conn *conn = u_user_conn(si->u);
			u_conn_f(conn, ":%U MODE %U %s%s",
			         si->u, si->u, stack.cbuf, stack.dbuf);
		}

		u_sendto_servers(si->link, ":%U MODE %U %s%s",
		                 si->u, si->u, stack.cbuf, stack.dbuf);
	}

	u_log(LG_VERBOSE, "%U now has user mode %s",
	      si->u, u_user_modes(si->u));

	return 0;
}

struct cmode_stacker_buf {
	int on;
	char *c, cbuf[512]; /* mode char buffer */
	char *d, dbuf[512]; /* mode data buffer */
};

struct cmode_stacker_private {
	struct cmode_stacker_buf u, s;
};

static void cmode_stacker_buf_init(struct cmode_stacker_buf *b)
{
	b->on = -1;
	b->c = b->cbuf;
	b->d = b->dbuf;
	*b->c = '\0';
	*b->d = '\0';
}

static void cmode_stacker_buf_put(struct cmode_stacker_buf *b, int on,
                                  char ch, char *param)
{
	if (b->on != on) {
		*b->c++ = on ? '+' : '-';
		b->on = on;
	}

	*b->c++ = ch;

	if (param != NULL)
		b->d += snprintf(b->d, b->dbuf + 512 - b->d, " %s", param);

	*b->c = '\0';
	*b->d = '\0';
}

static void cmode_stacker_start(u_modes *m)
{
	struct cmode_stacker_private *p = m->stack;
	cmode_stacker_buf_init(&p->u);
	cmode_stacker_buf_init(&p->s);
}

static void cmode_stacker_end(u_modes *m)
{
	u_chan *c = m->target;
	struct cmode_stacker_private *p = m->stack;

	if (p->u.on != -1) {
		u_sendto_chan(m->target, NULL, ST_USERS, ":%I MODE %C %s%s",
		              m->setter, c, p->u.cbuf, p->u.dbuf);
	}

	if (p->s.on != -1) {
		u_sendto_servers(m->setter->source, ":%I TMODE %u %C %s%s",
		                 m->setter, c->ts, c, p->s.cbuf, p->s.dbuf);
	}
}

static void cmode_stacker_put_external(u_modes *m, int on, char *param)
{
	struct cmode_stacker_private *p = m->stack;
	cmode_stacker_buf_put(&p->u, on, m->info->ch, param);
	cmode_stacker_buf_put(&p->s, on, m->info->ch, param);
}

static void cmode_stacker_put_status(u_modes *m, int on, void *tgt)
{
	struct cmode_stacker_private *p = m->stack;
	u_chanuser *cu = tgt;
	cmode_stacker_buf_put(&p->u, on, m->info->ch, cu->u->nick);
	cmode_stacker_buf_put(&p->s, on, m->info->ch, cu->u->uid);
}

static void cmode_stacker_put_flag(u_modes *m, int on)
{
	struct cmode_stacker_private *p = m->stack;
	cmode_stacker_buf_put(&p->u, on, m->info->ch, NULL);
	cmode_stacker_buf_put(&p->s, on, m->info->ch, NULL);
}

static void cmode_stacker_put_listent(u_modes *m, int on, u_listent *ban)
{
	struct cmode_stacker_private *p = m->stack;
	cmode_stacker_buf_put(&p->u, on, m->info->ch, ban->mask);
	cmode_stacker_buf_put(&p->s, on, m->info->ch, ban->mask);
}

static void cmode_stacker_send_list(u_modes *m)
{
	mowgli_list_t *list;

	if (!m->setter->u) {
		u_log(LG_WARN, "Tried to send list to non-user");
		return;
	}

	if (!(list = m->ctx->get_list(m, m->info))) {
		u_log(LG_SEVERE, "Can't send a list we don't have!!");
		return;
	}

	u_chan_send_list(m->target, m->setter->u, list);
}

static u_mode_stacker cmode_stacker = {
	.start          = cmode_stacker_start,
	.end            = cmode_stacker_end,

	.put_external   = cmode_stacker_put_external,
	.put_status     = cmode_stacker_put_status,
	.put_flag       = cmode_stacker_put_flag,
	.put_listent    = cmode_stacker_put_listent,

	.send_list      = cmode_stacker_send_list,
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
	struct cmode_stacker_private stack;

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
	m.flags = 0;

	if (SRC_IS_LOCAL_USER(si)) {
		u_chanuser *cu = u_chan_user_find(c, si->u);

		if (msg->argc == 1) {
			return u_user_num(si->u, RPL_CHANNELMODEIS, c,
					  u_chan_modes(c, !!cu));
		}

		if (cu->flags & CU_PFX_OP)
			m.access = cu;

	} else { /* source is local server or remote user/server */
		m.flags |= MODE_FORCE_ALL;

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
	m.stack = &stack;

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
	char *target = msg->argv[1];
	int parc;
	char **parv;
	u_chan *c;
	u_modes m;
	struct cmode_stacker_private stack;

	if (!(c = u_chan_get(target))) {
		return u_log(LG_ERROR, "%G tried to TMODE nonexistent %s",
		             si->source, target);
	}

	if (atoi(msg->argv[0]) > c->ts)
		return 0;

	parc = msg->argc - 2;
	parv = msg->argv + 2;

	m.ctx = &cmodes;
	m.stacker = &cmode_stacker;
	m.setter = si;
	m.target = c;
	m.access = &me;
	m.stack = &stack;

	u_mode_process(&m, parc, parv);

	return 0;
}

static u_cmd mode_cmdtab[] = {
	{ "MODE",  SRC_ANY,  c_a_mode,  1 },
	{ "TMODE", SRC_S2S,  c_r_tmode, 2 },
	{ }
};

TETHYS_MODULE_V1(
	"core/mode", "Alex Iadicicco", "MODE and TMODE commands",
	NULL, NULL, mode_cmdtab);

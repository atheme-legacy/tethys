/* Tethys, core/sjoin -- SJOIN command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

struct sjoin_stack {
	int nargs;
	int on;
	char *c, cbuf[512];
	char *d, dbuf[512];
};

static void sjoin_stack_reset(struct sjoin_stack *s)
{
	s->nargs = 0;
	s->on = -1;
	s->c = s->cbuf;
	s->d = s->dbuf;
}

static void sjoin_stacker_flush(u_modes *m)
{
	struct sjoin_stack *s = m->stack;

	*s->c = '\0';
	*s->d = '\0';

	if (s->on != -1) {
		u_sendto_chan(m->target, NULL, ST_USERS, ":%I MODE %C %s%s",
		              m->setter, m->target, s->cbuf, s->dbuf);
	}

	sjoin_stack_reset(s);
}

static void sjoin_stacker_put(u_modes *m, int on, char ch, char *param)
{
	struct sjoin_stack *s = m->stack;

	if (param != NULL && s->nargs == 4) /* XXX: hardcoded 4 */
		sjoin_stacker_flush(m);

	if (s->on != on) {
		*s->c++ = on ? '+' : '-';
		s->on = on;
	}

	*s->c++ = ch;

	if (param != NULL) {
		s->d += snprintf(s->d, s->dbuf + 512 - s->d, " %s", param);
		s->nargs++;
	}
}

static void sjoin_stacker_start(u_modes *m)
{
	sjoin_stack_reset(m->stack);
}

static void sjoin_stacker_end(u_modes *m)
{
	sjoin_stacker_flush(m);
}

static void sjoin_put_external(u_modes *m, int on, char *param)
{
	sjoin_stacker_put(m, on, m->info->ch, param);
}

static void sjoin_put_status(u_modes *m, int on, void *tgt)
{
	u_chanuser *cu = tgt;
	sjoin_stacker_put(m, on, m->info->ch, cu->u->nick);
}

static void sjoin_put_flag(u_modes *m, int on)
{
	sjoin_stacker_put(m, on, m->info->ch, NULL);
}

static void sjoin_put_listent(u_modes *m, int on, u_listent *ban)
{
	sjoin_stacker_put(m, on, m->info->ch, ban->mask);
}

static u_mode_stacker sjoin_stacker = {
	/* these are NULL because we will manually start and end the SJOIN
	   stacker from our SJOIN code */
	.start          = NULL,
	.end            = NULL,

	.put_external   = sjoin_put_external,
	.put_status     = sjoin_put_status,
	.put_flag       = sjoin_put_flag,
	.put_listent    = sjoin_put_listent,
};

static void get_status(u_chanuser *cu, int on, u_modes *m, char **p)
{
	if (cu->flags & CU_PFX_OP) {
		sjoin_stacker_put(m, on, 'o', cu->u->nick);
		if (p) *(*p)++ = '@';
	}

	if (cu->flags & CU_PFX_VOICE) {
		sjoin_stacker_put(m, on, 'v', cu->u->nick);
		if (p) *(*p)++ = '+';
	}

	if (p) *(*p) = '\0';
}

static uint parse_status(u_sourceinfo *si, char **sptr)
{
	char *s = *sptr;
	uint flags = 0;

	for (; *s && !isdigit(*s); s++) {
		switch (*s) {
		case '@':  flags |= CU_PFX_OP;    break;
		case '+':  flags |= CU_PFX_VOICE; break;
		default:
			u_log(LG_SEVERE, "%I used unk. prefix %c", si, *s);
		}
	}

	*sptr = s;

	return flags;
}

static void apply_modes(u_sourceinfo *si, u_chan *c, u_modes *m, u_msg *msg)
{
	u_mode_process(m, msg->argc - 3, msg->argv + 2);
}

static void join_uids(u_sourceinfo *si, u_chan *c, bool use_status,
                      u_modes *m, char *users, char *newusers)
{
	u_user *u;
	u_chanuser *cu;
	u_strop_state st;
	char *s, *p, *oldp;
	uint flags;

	p = newusers;

	U_STROP_SPLIT(&st, users, " ", &s) {
		flags = 0;

		oldp = p;
		if (p != newusers)
			*p++ = ' ';

		/* we need to call parse_status to advance s, however */
		flags = parse_status(si, &s);
		if (!use_status)
			flags = 0;

		if (!(u = u_user_by_uid(s))) {
			u_log(LG_ERROR, "%S tried to SJOIN nonexistent %s!",
			      si->s, s);
			p = oldp;
			continue;
		}

		if ((cu = u_chan_user_find(c, u))) {
			u_log(LG_WARN, "Already have chanuser %U/%C; ignoring",
			      u, c);
		} else if (!(cu = u_chan_user_add(c, u))) {
			u_log(LG_ERROR, "Could not create chanuser %U/%C", u, c);
			p = oldp;
			continue;
		}

		u_sendto_chan(c, NULL, ST_USERS, ":%H JOIN :%C", u, c);

		cu->flags |= flags;
		get_status(cu, 1, m, &p);
		p += sprintf(p, "%s", s);
	}
}

/* TSes are equal. Accept all simple modes and statuses */
static void ts_equal(u_sourceinfo *si, u_chan *c, u_modes *m, u_msg *msg)
{
	char users[512];

	u_log(LG_DEBUG, "ts_equal(%C)", c);

	apply_modes(si, c, m, msg);

	join_uids(si, c, true, m, msg->argv[msg->argc - 1], users);

	u_sendto_servers(si->source, ":%I SJOIN %u %C %s :%s",
	                 si, c->ts, c, u_chan_modes(c, 1), users);
}

/* Our TS is newer. Wipe all local modes and statuses */
static void ts_lose(u_sourceinfo *si, u_chan *c, u_modes *m, u_msg *msg)
{
	u_user *u;
	u_chanuser *cu;
	u_map_each_state st;
	char users[512];
	int ch;

	u_log(LG_DEBUG, "ts_lose(%C)", c);

	/* clear all modes */
	for (ch=0; ch<128; ch++) {
		m->info = &cmode_infotab[ch];
		if (!m->info->ch)
			continue;

		switch (m->info->type) {
		case MODE_EXTERNAL:
			/* XXX: using "*" here is not portable */
			m->info->arg.fn(m, 0, "*");
			break;

		case MODE_FLAG:
			if (c->mode & m->info->arg.data)
				m->stacker->put_flag(m, 0);
			break;

		default:
			/* status cleared in next step */
			/* lists only ever merged, I guess */
			break;
		}
	}
	c->mode = 0;

	/* remove all statuses from local users */
	U_MAP_EACH(&st, c->members, &u, &cu) {
		u_conn *link;

		if (!IS_LOCAL_USER(u))
			continue;
		link = u_user_conn(u);

		get_status(cu, 0, m, NULL);
		cu->flags = 0;
	}

	apply_modes(si, c, m, msg);

	join_uids(si, c, true, m, msg->argv[msg->argc - 1], users);

	u_sendto_servers(si->source, ":%I SJOIN %u %C %s :%s",
	                 si, c->ts, c, u_chan_modes(c, 1), users);
}

/* Our TS is older. Ignore all simple modes and statuses */
static void ts_win(u_sourceinfo *si, u_chan *c, u_modes *m, u_msg *msg)
{
	char users[512];

	u_log(LG_DEBUG, "ts_win(%C)", c);

	join_uids(si, c, false, m, msg->argv[msg->argc - 1], users);

	u_sendto_servers(si->source, ":%I SJOIN %u %C %s :%s",
	                 si, c->ts, c, u_chan_modes(c, 1), users);
}

static int ts_rules(u_sourceinfo *si, u_chan *c, int ts, u_msg *msg)
{
	u_modes m;
	struct sjoin_stack stack;

	sjoin_stack_reset(&stack);

	m.ctx = &cmodes;
	m.stacker = &sjoin_stacker;
	m.setter = si;
	m.target = c;
	m.access = NULL;
	m.flags = MODE_FORCE_ALL;
	m.stack = &stack;

	if (c->ts == 0 || ts == 0) {
		u_sendto_chan(c, NULL, ST_USERS,
		              ":%S NOTICE %C :TS changed from %d to 0",
		              &me, c, c->ts);
		c->ts = 0;
		ts_equal(si, c, &m, msg);
		sjoin_stacker_flush(&m);
		return 0;
	}

	if (c->ts == ts) {
		ts_equal(si, c, &m, msg);
	} else if (c->ts < ts) { /* we are older */
		ts_win(si, c, &m, msg);
	} else if (ts < c->ts) { /* we are newer */
		u_sendto_chan(c, NULL, ST_USERS,
		              ":%S NOTICE %C :TS changed from %d to %d",
		              &me, c, c->ts, ts);
		c->ts = ts;
		ts_lose(si, c, &m, msg);
	}

	sjoin_stacker_flush(&m);

	return 0;
}

static int c_s_sjoin(u_sourceinfo *si, u_msg *msg)
{
	u_chan *c;
	char *channame = msg->argv[1];
	int ts = atoi(msg->argv[0]);

	if ((c = u_chan_get(channame)) != NULL)
		return ts_rules(si, c, ts, msg);

	c = u_chan_create(channame);
	c->ts = ts;
	ts_rules(si, c, c->ts, msg);
}

static u_cmd sjoin_cmdtab[] = {
	{ "SJOIN", SRC_SERVER, c_s_sjoin, 4 },
	{ }
};

TETHYS_MODULE_V1(
	"core/sjoin", "Alex Iadicicco", "SJOIN command",
	NULL, NULL, sjoin_cmdtab);

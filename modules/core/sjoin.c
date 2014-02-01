/* Tethys, core/sjoin -- SJOIN command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void send_flags(u_sourceinfo *setter, int on, char *mbuf,
                       u_chan *c, u_user *u)
{
	on = on ? '+' : '-';

	if (mbuf[0] && !mbuf[1]) {
		u_sendto_chan(c, NULL, ST_USERS,
			      ":%I MODE %C %c%s %s",
			      setter, c, on, mbuf, u->nick);
	} else if (mbuf[0] && mbuf[1]) {
		u_sendto_chan(c, NULL, ST_USERS,
			      ":%I MODE %C %c%s %s %s",
			      setter, c, on, mbuf, u->nick, u->nick);
	}
}

static void get_status(uint flags, char *m, char *p)
{
	if (flags & CU_PFX_OP) {
		*p++ = '@';
		*m++ = 'o';
	}

	if (flags & CU_PFX_VOICE) {
		*p++ = '+';
		*m++ = 'v';
	}

	*p = '\0';
	*m = '\0';
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

static void apply_modes(u_sourceinfo *si, u_chan *c, u_msg *msg)
{
	u_modes m;

	m.setter = si;
	m.target = c;
	m.perms = &me;
	m.flags = 0;
	u_mode_process(&m, cmodes, msg->argc - 3, msg->argv + 2);

	if (m.u.buf[0] || m.u.data[0]) {
		u_sendto_chan(c, NULL, ST_USERS, ":%I MODE %C %s%s",
		              si, c, m.u.buf, m.u.data);
	}
}

static void join_uids(u_sourceinfo *si, u_chan *c, bool use_status,
                      char *users, char *newusers)
{
	u_user *u;
	u_chanuser *cu;
	u_strop_state st;
	char *s, *p, *oldp;
	char mbuf[16];
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

		get_status(flags, mbuf, p);
		p += sprintf(p, "%s", s);
		cu->flags |= flags;
		u_sendto_chan(c, NULL, ST_USERS, ":%H JOIN :%C", u, c);
		send_flags(si, 1, mbuf, c, u);
	}
}

/* TSes are equal. Accept all simple modes and statuses */
static void ts_equal(u_sourceinfo *si, u_chan *c, u_msg *msg)
{
	char users[512];

	u_log(LG_DEBUG, "ts_equal(%C)", c);

	apply_modes(si, c, msg);

	join_uids(si, c, true, msg->argv[msg->argc - 1], users);

	u_sendto_servers(si->source, ":%I SJOIN %u %C %s :%s",
	                 si, c->ts, c, u_chan_modes(c, 1), users);
}

/* Our TS is newer. Wipe all local modes and statuses */
static void ts_lose(u_sourceinfo *si, u_chan *c, u_msg *msg)
{
	u_user *u;
	u_chanuser *cu;
	u_map_each_state st;
	char users[512];

	u_log(LG_DEBUG, "ts_lose(%C)", c);

	/* TODO: calculate mode difference? */
	c->mode = 0;

	/* remove all statuses from local users */
	U_MAP_EACH(&st, c->members, &u, &cu) {
		char mbuf[4], pbuf[4];
		u_conn *link;

		if (!IS_LOCAL_USER(u))
			continue;
		link = u_user_conn(u);

		get_status(cu->flags, mbuf, pbuf);
		cu->flags = 0;
		send_flags(si, 0, mbuf, c, u);

		/* this is a terrible way to do this */
		u_user_num(u, RPL_CHANNELMODEIS, c, "+");
	}

	apply_modes(si, c, msg);

	join_uids(si, c, true, msg->argv[msg->argc - 1], users);

	u_sendto_servers(si->source, ":%I SJOIN %u %C %s :%s",
	                 si, c->ts, c, u_chan_modes(c, 1), users);
}

/* Our TS is older. Ignore all simple modes and statuses */
static void ts_win(u_sourceinfo *si, u_chan *c, u_msg *msg)
{
	char users[512];

	u_log(LG_DEBUG, "ts_win(%C)", c);

	join_uids(si, c, false, msg->argv[msg->argc - 1], users);

	u_sendto_servers(si->source, ":%I SJOIN %u %C %s :%s",
	                 si, c->ts, c, u_chan_modes(c, 1), users);
}

static int ts_rules(u_sourceinfo *si, u_chan *c, int ts, u_msg *msg)
{
	if (c->ts == 0 || ts == 0) {
		u_sendto_chan(c, NULL, ST_USERS,
		              ":%S NOTICE %C :TS changed from %d to 0",
		              &me, c, c->ts);
		c->ts = 0;
		ts_equal(si, c, msg);
		return 0;
	}

	if (c->ts == ts) {
		ts_equal(si, c, msg);
	} else if (c->ts < ts) { /* we are older */
		ts_win(si, c, msg);
	} else if (ts < c->ts) { /* we are newer */
		u_sendto_chan(c, NULL, ST_USERS,
		              ":%S NOTICE %C :TS changed from %d to %d",
		              &me, c, c->ts, ts);
		c->ts = ts;
		ts_lose(si, c, msg);
	}

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
	ts_equal(si, c, msg);
}

static u_cmd sjoin_cmdtab[] = {
	{ "SJOIN", SRC_SERVER, c_s_sjoin, 4 },
	{ }
};

TETHYS_MODULE_V1(
	"core/sjoin", "Alex Iadicicco", "SJOIN command",
	NULL, NULL, sjoin_cmdtab);

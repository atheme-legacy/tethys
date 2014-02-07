/* Tethys, core/stats -- STATS command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void notice(u_sourceinfo *si, const char *fmt, ...)
{
	char buf[512];
	va_list va;

	if (!SRC_IS_USER(si))
		return;

	va_start(va, fmt);
	vsnf(FMT_USER, buf, 512, fmt, va);
	va_end(va);

	u_conn_f(si->link, ":%S NOTICE %U :%s", &me, si->u, buf);
}

#define NEED_OPER   0x01

struct stats_info {
	char *name;
	uint need;
	void (*cb)(u_sourceinfo*, struct stats_info*);
};

static void stats_o(u_sourceinfo *si, struct stats_info *info)
{
	u_map_each_state state;
	char *k;
	u_oper *o;
	char *auth;

	U_MAP_EACH(&state, all_opers, &k, &o) {
		auth = o->authname[0] ? o->authname : "<any>";
		u_src_num(si, RPL_STATSOLINE, o->name, o->pass, auth);
	}
}

static void stats_i(u_sourceinfo *si, struct stats_info *info)
{
	u_map_each_state state;
	char buf[CIDR_ADDRSTRLEN];
	char *k;
	u_auth *v;

	U_MAP_EACH(&state, all_auths, &k, &v) {
		u_cidr_to_str(&v->cidr, buf);
		u_src_num(si, RPL_STATSILINE, v->name, v->classname, buf);
	}
}

static void stats_u(u_sourceinfo *si, struct stats_info *info)
{
	int days, hr, min, sec;

	sec = NOW.tv_sec - started;

	min  = sec / 60;   sec %= 60;
	hr   = min / 60;   min %= 60;
	days = hr  / 24;   hr  %= 24;

	u_src_num(si, RPL_STATSUPTIME, days, hr, min, sec);
}

static void do_command(u_sourceinfo *si, u_cmd *cmd)
{
	char mask[15], *prop;
	char usecs[15];
	int i;

	for (i=0; i<12; i++)
		mask[13 - i - (i >> 2)] = (cmd->mask & (1 << i)) ? 'X' : '.';
	mask[4] = mask[9] = ' ';
	mask[14] = '\0';

	prop = "???";
	switch (cmd->propagation) {
	case CMD_PROP_NONE:        prop = "   "; break;
	case CMD_PROP_BROADCAST:   prop = "brd"; break;
	case CMD_PROP_ONE_TO_ONE:  prop = "1-1"; break;
	case CMD_PROP_HUNTED:      prop = "hnt"; break;
	}

	usecs[0] = '-';
	usecs[1] = '\0';
	if (cmd->runs > 0)
		snprintf(usecs, 15, "%d,%dus", cmd->runs, cmd->usecs / cmd->runs);

	notice(si, "%16s  %s  %2d  %s  %15s  module %s", cmd->name, mask,
	       cmd->nargs, prop, usecs,
	       cmd->owner ? cmd->owner->info->name : "(none)");
}

static void stats_commands(u_sourceinfo *si, struct stats_info *info)
{
	mowgli_patricia_iteration_state_t state;
	u_cmd *cmd;

	notice(si, "                    UU EERL RLRL");
	notice(si, "                   FSU SUSS UUOO");
	MOWGLI_PATRICIA_FOREACH(cmd, &state, all_commands) {
		MOWGLI_ITER_FOREACH(cmd, cmd) {
			do_command(si, cmd);
		}
	}
}

static void field(char *s, size_t sz, const char *src, char fill)
{
	size_t r;
	char space = ' ';

	r = snprintf(s, sz, "%s", src);

	if (r >= sz || sz - r < 4) {
		strcpy(s + sz - 4, "...");
		return;
	}

	while (r < sz) {
		s[r++] = space;
		space = fill;
	}

	s[sz - 1] = '\0';
}

static void stats_modules(u_sourceinfo *si, struct stats_info *info)
{
	mowgli_patricia_iteration_state_t state;
	u_module *m;
	u_cmd *cmd;

	MOWGLI_PATRICIA_FOREACH(m, &state, u_modules) {
		char name[20];
		char author[20];
		char desc[40];

		field(name, 20, m->info->name, ' ');
		field(author, 20, m->info->author, ' ');
		field(desc, 40, m->info->description, '\0');

		notice(si, "\2%s\2 %s %s", name, author, desc);
	}
}

struct stats_info stats[] = {
	{ "o", NEED_OPER, stats_o },
	{ "i", NEED_OPER, stats_i },
	{ "u", 0,         stats_u },

	/* extended stats */
	{ "commands", NEED_OPER, stats_commands },
	{ "modules",  NEED_OPER, stats_modules  },

	{ }
};

static int c_u_stats(u_sourceinfo *si, u_msg *msg)
{
	struct stats_info *info;
	char *name = msg->argv[0];
	u_server *sv;

	if (!*name) /* "STATS :" will do this */
		return u_src_num(si, ERR_NEEDMOREPARAMS, "STATS");

	if (msg->argc > 1) {
		if (!(sv = u_server_by_ref(si->source, msg->argv[1])))
			return u_src_num(si, ERR_NOSUCHSERVER, msg->argv[1]);

		if (sv != &me) {
			u_conn_f(sv->conn, ":%I STATS %s %S", si, name, sv);
			return 0;
		}
	}

	for (info=stats; info->name; info++) {
		if (info->name[0] && !info->name[1]) {
			/* exact match for 1 char stats */
			if (!streq(info->name, name))
				continue;
		} else {
			/* caseless match for 2+ char stats */
			if (casecmp(info->name, name))
				continue;
		}

		if ((info->need & NEED_OPER) && !(si->u->mode & UMODE_OPER)) {
			u_src_num(si, ERR_NOPRIVILEGES);
			break;
		}

		info->cb(si, info);
		break;
	}

	if (!info->name) /* HACK ALERT! */
		name[1] = '\0'; /* HACK ALERTTTT! */

	u_src_num(si, RPL_ENDOFSTATS, name);
	return 0;
}

static u_cmd stats_cmdtab[] = {
	{ "STATS", SRC_USER, c_u_stats, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/stats", "Alex Iadicicco", "STATS command",
	NULL, NULL, stats_cmdtab);

/* Tethys, core/stats -- STATS command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

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

struct stats_info stats[] = {
	{ "o", NEED_OPER, stats_o },
	{ "i", NEED_OPER, stats_i },
	{ "u", 0,         stats_u },
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
		if (!(sv = u_server_by_ref(msg->argv[1])))
			return u_src_num(si, ERR_NOSUCHSERVER, msg->argv[1]);

		if (sv != &me) {
			u_conn_f(sv->conn, ":%I STATS %s %S", si, name, sv);
			return;
		}
	}

	for (info=stats; info->name; info++) {
		if (!streq(info->name, name))
			continue;

		if ((info->need & NEED_OPER) && !(si->u->flags & UMODE_OPER)) {
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

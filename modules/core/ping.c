/* Tethys, core/ping -- PING and PONG commands
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* TODO: reimplement this with CMD_PROP_ONE_TO_ONE */
static int c_a_ping(u_sourceinfo *si, u_msg *msg)
{
	u_conn *conn = si->source;
	u_server *sv;
	char *tgt = msg->argv[1];

	if (msg->command[1] == '0') /* local user PONG */
		return 0;

	/* I hate this command so much  --aji */

	if (!tgt || !*tgt) {
		u_conn_f(conn, ":%S PONG %s :%s", &me, me.name, msg->argv[0]);
		return 0;
	}

	if (!(sv = u_server_by_name(msg->argv[1]))) {
		if (conn->ctx == CTX_USER) {
			u_conn_num(conn, ERR_NOSUCHSERVER, tgt);
		} else {
			u_log(LG_ERROR, "%S sent PING for nonexistent %s",
			      si->s, tgt);
		}
		return 0;
	}

	if (sv == &me) {
		u_conn_f(conn, ":%S PONG %s %s", &me, me.name, si->name);
		return 0;
	}

	if (sv->conn == si->source)
		return u_log(LG_ERROR, "%G sent PING for wrong subtree", conn);

	u_conn_f(sv->conn, ":%s PING %s %s", si->id, si->name, sv->name);

	return 0;
}

/* TODO: reimplement this with CMD_PROP_ONE_TO_ONE */
static int c_s_pong(u_sourceinfo *si, u_msg *msg)
{
	char *name, *dest = msg->argv[1];
	u_conn *link;

	if (strchr(dest, '.')) {
		u_server *sv = u_server_by_name(dest);
		link = sv ? sv->conn : NULL;
		name = sv ? sv->name : NULL;

		if (sv == &me) {
			if (si->s->flags & SERVER_IS_BURSTING)
				u_server_eob(si->s);
			return 0;
		}
	} else {
		u_user *u = u_user_by_nick(dest);
		link = u_user_conn(u);
		name = u ? u->nick : NULL;
	}

	if (!link || !name) {
		u_log(LG_ERROR, "%G sent PONG for nonexistent %s",
		      si->source, dest);
		return 0;
	}

	u_conn_f(link, ":%S PONG %s %s", si->s, si->s->name, name);

	return 0;
}

static u_cmd ping_cmdtab[] = {
	{ "PING", SRC_ANY,         c_a_ping, 1 },
	{ "PONG", SRC_LOCAL_USER,  c_a_ping, 0 },
	{ "PONG", SRC_SERVER,      c_s_pong, 2 },
	{ }
};

static int init_ping(u_module *m)
{
	u_cmds_reg(ping_cmdtab);
}

TETHYS_MODULE_V1(
	"core/ping", "Alex Iadicicco", "PING and PONG commands",
	init_ping, NULL);

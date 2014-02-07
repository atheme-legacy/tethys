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

	if (msg->command[1] == 'O') /* local user PONG */
		return 0;

	/* I hate this command so much  --aji */

	if (!tgt || !*tgt) {
		sv = u_server_by_ref(si->source, msg->argv[0]);
		tgt = sv ? sv->sid : msg->argv[0]; /* idk */
		u_conn_f(conn, ":%S PONG %s :%s", &me, me.name, tgt);
		return 0;
	}

	if (!(sv = u_server_by_ref(conn, tgt))) {
		if (conn->ctx == CTX_USER) {
			u_conn_num(conn, ERR_NOSUCHSERVER, tgt);
		} else {
			u_log(LG_ERROR, "%S sent PING for nonexistent %s",
			      si->s, tgt);
		}
		return 0;
	}

	if (sv == &me) {
		u_conn_f(conn, ":%S PONG %s :%s", &me, me.name,
		         SRC_IS_LOCAL_USER(si) ? si->name : si->id);
		return 0;
	}

	if (sv->link == si->source)
		return u_log(LG_ERROR, "%G sent PING for wrong subtree", conn);

	u_conn_f(sv->link, ":%s PING %s :%s", si->id, si->name, sv->sid);

	return 0;
}

/* TODO: reimplement this with CMD_PROP_ONE_TO_ONE */
static int c_s_pong(u_sourceinfo *si, u_msg *msg)
{
	char *name, *dest = msg->argv[1];
	u_conn *link;

	link = ref_link(si->source, dest);
	name = ref_to_ref(si->source, dest);

	if (!link && name) { /* PONG to me */
		u_log(LG_VERBOSE, "PONG to me");
		if (si->s->flags & SERVER_IS_BURSTING)
			u_server_eob(si->s);
		return 0;
	}

	if (!name) {
		u_log(LG_ERROR, "%G sent PONG for nonexistent %s",
		      si->source, dest);
		return 0;
	}

	u_conn_f(link, ":%S PONG %s :%s", si->s, si->s->name, name);

	return 0;
}

static u_cmd ping_cmdtab[] = {
	{ "PING", SRC_ANY,         c_a_ping, 1 },
	{ "PONG", SRC_LOCAL_USER,  c_a_ping, 0 },
	{ "PONG", SRC_SERVER,      c_s_pong, 2 },
	{ }
};

TETHYS_MODULE_V1(
	"core/ping", "Alex Iadicicco", "PING and PONG commands",
	NULL, NULL, ping_cmdtab);

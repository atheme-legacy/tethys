/* ircd-micro, c_hunted.c -- commands with hunted parameters
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void m_ping(conn, msg) u_conn *conn; u_msg *msg;
{
	u_server *sv;
	u_user *u;
	char *tgt, *origin;

	if (msg->command[1] == 'O') /* user PONG */
		return;

	if (msg->argc == 1) {
		u_conn_f(conn, ":%S PONG %s :%s", &me, me.name, msg->argv[0]);
		return;
	}

	if (streq(msg->argv[1], me.name)) {
		tgt = NULL;

		if (conn->ctx == CTX_USER) {
			u = conn->priv;
			tgt = u->nick;
		} else {
			tgt = id_to_name(msg->source);
		}

		if (tgt == NULL)
			u_log(LG_ERROR, "Useless PING from %G", conn);
		else
			u_conn_f(conn, ":%S PONG %s %s", &me, me.name, tgt);

		return;
	}

	if (!(sv = u_server_by_name(msg->argv[1]))) {
		if (conn->ctx == CTX_USER)
			u_conn_num(conn, ERR_NOSUCHSERVER, msg->argv[1]);
		else
			u_log(LG_ERROR, "%G sent PING for nonexistent %s",
			      conn, msg->argv[1]);
		return;
	}

	origin = msg->source;
	if (origin == NULL || conn->ctx == CTX_USER)
		origin = conn_id(conn);

	u_conn_f(sv->conn, ":%s PING %s %s", ref_to_id(origin),
	         ref_to_name(origin), msg->argv[1]);
}

static void m_pong(conn, msg) u_conn *conn; u_msg *msg;
{
	u_server *from;
	u_conn *to;

	if (!(from = u_server_by_sid(msg->source)))
		return;

	if (streq(msg->argv[1], me.name)) {
		if (conn->ctx == CTX_SBURST)
			u_server_eob(conn->priv);
		return;
	}

	to = u_conn_by_name(msg->argv[1]);
	if (to == NULL) {
		u_log(LG_ERROR, "%G sent PONG for nonexistent %s",
		      conn, msg->argv[1]);
		return;
	}

	if (from->conn == to)
		return;

	u_conn_f(to, ":%S PONG %s %s", from, from->name, msg->argv[1]);
}

u_cmd c_hunted[] = {
	{ "PING",        CTX_USER,    m_ping,        1 },
	{ "PONG",        CTX_USER,    m_ping,        0 },

	{ "PING",        CTX_SBURST,  m_ping,        1 },
	{ "PING",        CTX_SERVER,  m_ping,        1 },
	{ "PONG",        CTX_SBURST,  m_pong,        2 },
	{ "PONG",        CTX_SERVER,  m_pong,        2 },

	{ "" }
};

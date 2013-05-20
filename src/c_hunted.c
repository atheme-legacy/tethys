/* ircd-micro, c_hunted.c -- commands with hunted parameters
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void m_ping(conn, msg) u_conn *conn; u_msg *msg;
{
	u_server *sv;

	if (msg->command[1] == 'O') /* user PONG */
		return;

	if (msg->argc == 1) {
		u_conn_f(conn, ":%S PONG %s :%s", &me, me.name, msg->argv[0]);
		return;
	}

	if (streq(msg->argv[1], me.name)) {
		if (msg->src == NULL)
			u_log(LG_ERROR, "Useless PING from %G", conn);
		else
			u_conn_f(conn, ":%S PONG %s %s", &me, me.name,
			         msg->src->name);
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

	if (msg->src != NULL)
		u_conn_f(sv->conn, ":%s PING %s %s", msg->src->id,
			 msg->src->name, sv->name);
}

static void m_pong(conn, msg) u_conn *conn; u_msg *msg;
{
	u_server *from, *sv = conn->priv;
	u_conn *to;

	if (!msg->src || !ENT_IS_SERVER(msg->src))
		return;
	from = msg->src->v.sv;

	if (streq(msg->argv[1], me.name)) {
		if (sv->flags & SERVER_IS_BURSTING)
			u_server_eob(conn->priv);
		return;
	}

	to = u_conn_by_name(msg->argv[1]);
	if (to == NULL) {
		u_log(LG_ERROR, "%G sent PONG for nonexistent %s",
		      conn, msg->argv[1]);
		return;
	}

	if (sv->conn == to)
		return;

	u_conn_f(to, ":%S PONG %s %s", from, from->name, msg->argv[1]);
}

u_cmd c_hunted[] = {
	{ "PING",        CTX_USER,    m_ping,        1 },
	{ "PONG",        CTX_USER,    m_ping,        0 },
	{ "PING",        CTX_SERVER,  m_ping,        1 },
	{ "PONG",        CTX_SERVER,  m_pong,        2 },

	{ "" }
};

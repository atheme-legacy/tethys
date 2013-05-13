/* ircd-micro, c_hunted.c -- commands with hunted parameters
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void m_ping(conn, msg) u_conn *conn; u_msg *msg;
{
	u_server *sv;
	u_user *u;
	char *tgt;

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
			switch(strlen(msg->source)) {
			case 9:
				u = u_user_by_uid(msg->source);
				if (u != NULL)
					tgt = u->nick;
				break;
			case 3:
				sv = u_server_by_sid(msg->source);
				if (sv != NULL)
					tgt = sv->name;
				break;
			}
		}

		if (tgt == NULL)
			u_log(LG_ERROR, "Useless PING", conn->priv);
		else
			u_conn_f(conn, ":%S PONG %s %s", &me, me.name, tgt);

		return;
	}

	if (!(sv = u_server_by_name(msg->argv[1]))) {
		if (conn->ctx == CTX_USER)
			u_conn_num(conn, ERR_NOSUCHSERVER, msg->argv[1]);
		u_log(LG_ERROR, "%G sent PING for nonexistent %s",
		      conn, msg->argv[1]);
		return;
	}

	u_conn_f(sv->conn, ":%G PING %s %s",
	         conn, msg->argv[0], msg->argv[1]);
}

static void m_pong(conn, msg) u_conn *conn; u_msg *msg;
{
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

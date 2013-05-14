/* ircd-micro, c_message.c -- message propagation
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int message_blocked(c, u) u_chan *c; u_user *u;
{
	u_chanuser *cu = u_chan_user_find(c, u);

	if (!cu) {
		if (c->mode & CMODE_NOEXTERNAL) {
			u_user_num(u, ERR_CANNOTSENDTOCHAN, c, 'n');
			return 1;
		}
	} else if (u_is_muted(cu)) {
		/* TODO: +z */
		u_user_num(u, ERR_CANNOTSENDTOCHAN, c, 'm');
		return 1;
	}

	return 0;
}

static void m_message_chan(conn, msg) u_conn *conn; u_msg *msg;
{
	u_chan *tgt;

	if (!(tgt = u_chan_get(msg->argv[0])))
		return u_conn_num(conn, ERR_NOSUCHCHANNEL, msg->argv[0]);

	if (conn->ctx == CTX_USER && message_blocked(tgt, conn->priv))
		return;

	u_log(LG_DEBUG, "[%E -> %C] %s", msg->src, tgt, msg->argv[1]);

	if (ENT_IS_SERVER(msg->src)) {
		u_sendto_chan(tgt, conn, ":%S NOTICE %C :%s",
		              msg->src->v.sv, tgt, msg->argv[1]);
		return;
	}

	u_sendto_chan(tgt, conn, ":%H %s %C :%s", msg->src->v.u,
	              msg->command, tgt, msg->argv[1]);
}

static void m_message_user(conn, msg) u_conn *conn; u_msg *msg;
{
	u_entity tgt;

	/* XXX: make sure to test sending messages to servers */
	if (!u_entity_from_ref(&tgt, msg->argv[0]) || ENT_IS_SERVER(&tgt))
		return u_conn_num(conn, ERR_NOSUCHNICK, msg->argv[0]);

	u_log(LG_DEBUG, "[%E -> %E] %s", msg->src, &tgt, msg->argv[1]);

	if (ENT_IS_SERVER(msg->src)) {
		u_conn_f(tgt.link, ":%S NOTICE %U :%s", msg->src->v.sv,
		         tgt.v.u, msg->argv[1]);
		return;
	}

	u_conn_f(tgt.link, ":%H %s %U :%s", msg->src->v.u,
	         msg->command, tgt.v.u, msg->argv[1]);
}

static void m_message(conn, msg) u_conn *conn; u_msg *msg;
{
	if (ENT_IS_SERVER(msg->src) && msg->command[0] == 'P') {
		u_log(LG_ERROR, "%E tried to send a PRIVMSG!", msg->src);
		return;
	}

	if (msg->src == NULL) {
		u_log(LG_ERROR, "%s has no source!", msg->command);
		return;
	}

	if (msg->argv[0][0] == '#')
		m_message_chan(conn, msg);
	else
		m_message_user(conn, msg);
}

u_cmd c_message[] = {
	{ "PRIVMSG",   CTX_USER,     m_message, 2 },
	{ "NOTICE",    CTX_USER,     m_message, 2 },
	{ "PRIVMSG",   CTX_SERVER,   m_message, 2 },
	{ "NOTICE",    CTX_SERVER,   m_message, 2 },

	{ "" },
};

/* ircd-micro, c_message.c -- message propagation
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void m_message_chan(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *src = conn->priv;
	u_chan *tgt;
	u_chanuser *cu;

	if (!(tgt = u_chan_get(msg->argv[0])))
		return u_user_num(src, ERR_NOSUCHCHANNEL, msg->argv[0]);

	cu = u_chan_user_find(tgt, src);
	if (!cu) {
		if (tgt->mode & CMODE_NOEXTERNAL)
			return u_user_num(src, ERR_CANNOTSENDTOCHAN, tgt, 'n');
	} else if (u_is_muted(cu)) {
		/* TODO: +z */
		u_user_num(src, ERR_CANNOTSENDTOCHAN, tgt, 'm');
		return;
	}

	u_log(LG_DEBUG, "[%U -> %C] %s", src, tgt, msg->argv[1]);

	u_sendto_chan(tgt, conn, ":%H %s %C :%s", src, msg->command,
	              tgt, msg->argv[1]);
}

static void m_message_user(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *src = conn->priv;
	u_user *tgt;

	if (!(tgt = u_user_by_nick(msg->argv[0])))
		return u_user_num(src, ERR_NOSUCHNICK, msg->argv[0]);

	u_log(LG_DEBUG, "[%U -> %U] %s", src, tgt, msg->argv[1]);

	if (tgt->flags & USER_IS_LOCAL) {
		u_conn_f(((u_user_local*)tgt)->conn,
		         ":%H %s %U :%s", src, msg->command, tgt, msg->argv[1]);
	} else {
		u_user_num(src, ERR_GENERIC, "Can't send messages to remote users yet");
	}
}

static void m_message(conn, msg) u_conn *conn; u_msg *msg;
{
	if (msg->argv[0][0] == '#')
		m_message_chan(conn, msg);
	else
		m_message_user(conn, msg);
}

u_cmd c_message[] = {
	{ "PRIVMSG",   CTX_USER, m_message, 2 },
	{ "NOTICE",    CTX_USER, m_message, 2 },

	{ "" },
};

/* ircd-micro, c_user.c -- user commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* XXX this is wrong */
static void m_ping(conn, msg) u_conn *conn; u_msg *msg;
{
	if (msg->command[1] == 'O') /* PONG */
		return;

	u_conn_f(conn, ":%s PONG %s :%s", me.name, me.name, msg->argv[0]);
}

static void m_version(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *u = conn->priv;
	u_user_num(u, RPL_VERSION, PACKAGE_FULLNAME, me.name, "hi");
}

static void m_motd(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *u = conn->priv;
	u_user_send_motd(u);
}

static void m_message_chan(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *src = conn->priv;
	u_chan *tgt;

	tgt = u_chan_get(msg->argv[0]);
	if (tgt == NULL) {
		u_user_num(src, ERR_NOSUCHCHANNEL, msg->argv[0]);
		return;
	}

	u_log(LG_DEBUG, "[%s -> %s] %s", src->nick, tgt->name, msg->argv[1]);

	u_sendto_chan(tgt, conn, ":%s!%s@%s %s %s :%s", src->nick, src->ident,
	              src->host, msg->command, tgt->name, msg->argv[1]);
}

static void m_message_user(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *src = conn->priv;
	u_user *tgt;

	tgt = u_user_by_nick(msg->argv[0]);
	if (tgt == NULL) {
		u_user_num(src, ERR_NOSUCHNICK, msg->argv[0]);
		return;
	}

	u_log(LG_DEBUG, "[%s -> %s] %s", src->nick, tgt->nick, msg->argv[1]);

	if (tgt->flags & USER_IS_LOCAL) {
		u_conn_f(((u_user_local*)tgt)->conn,
		         ":%s!%s@%s %s %s :%s", src->nick, src->ident, src->host,
		         msg->command, tgt->nick, msg->argv[1]);
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

static void m_join(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *u = conn->priv;
	u_chan *c;
	u_chanuser *cu;

	c = u_chan_get_or_create(msg->argv[0]);

	if (c == NULL) {
		u_user_num(u, ERR_GENERIC, "Can't get or create channel!");
		return;
	}

	/* TODO: verify entry */

	cu = u_chan_user_add(c, u);

	if (c->members->size == 1) {
		u_log(LG_DEBUG, "Channel %s created", c->name);
		cu->flags |= CU_PFX_OP;
	}

	u_map_set(u->channels, c, cu);

	u_sendto_chan(c, NULL, ":%s!%s@%s JOIN %s", u->nick, u->ident, u->host, c->name);
	u_conn_f(conn, ":%s MODE %s %s", me.name, c->name, u_chan_modes(c));
	u_chan_send_topic(c, u);
	u_chan_send_names(c, u);
}

static void m_topic(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *u = conn->priv;
	u_chan *c;

	c = u_chan_get(msg->argv[0]);
	if (c == NULL) {
		u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);
		return;
	}

	if (msg->argc == 1) {
		u_chan_send_topic(c, u);
		return;
	}

	/* TODO: access checks */

	u_strlcpy(c->topic, msg->argv[1], MAXTOPICLEN+1);
	u_strlcpy(c->topic_setter, u->nick, MAXNICKLEN+1);
	c->topic_time = NOW.tv_sec;

	u_sendto_chan(c, NULL, ":%s!%s@%s TOPIC %s :%s", u->nick, u->ident,
	              u->host, c->name, c->topic);
}

static void m_names(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *u = conn->priv;
	u_chan *c;

	/* TODO: no arguments version */
	if (msg->argc == 0)
		return;

	c = u_chan_get(msg->argv[0]);
	if (c == NULL) {
		u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);
		return;
	}

	u_chan_send_names(c, u);
}

u_cmd c_user[] = {
	{ "PING",    CTX_USER, m_ping,    1 },
	{ "PONG",    CTX_USER, m_ping,    0 },
	{ "VERSION", CTX_USER, m_version, 0 },
	{ "MOTD",    CTX_USER, m_motd,    0 },
	{ "PRIVMSG", CTX_USER, m_message, 2 },
	{ "NOTICE",  CTX_USER, m_message, 2 },
	{ "JOIN",    CTX_USER, m_join,    1 },
	{ "TOPIC",   CTX_USER, m_topic,   1 },
	{ "NAMES",   CTX_USER, m_names,   0 },
	{ "" },
};

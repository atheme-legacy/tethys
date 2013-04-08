/* ircd-micro, c_user.c -- user commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

int ga_argc = 0;
static char **ga_argv;
static char *getarg(void)
{
	if (ga_argc <= 0)
		return NULL;
	ga_argc--;
	return *ga_argv++;
}

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
	char *keys[128], **keys_p;
	char *s, *p;
	u_user *u = conn->priv;
	int i;

	p = msg->argv[1];
	for (i=0; i<128; i++)
		keys[i] = cut(&p, ",");
	keys_p = keys;

	p = msg->argv[0];
	while ((s = cut(&p, ",")) != NULL) {
		u_log(LG_FINE, "  %s$%s", s, p);
		u_log(LG_FINE, "    key=%s", *keys_p);

		if (s[0] != '#') {
			if (*s) u_user_num(u, ERR_NOSUCHCHANNEL, s);
			continue;
		}

		u_user_try_join_chan(u, s, *keys_p++);
	}
}

static void m_part(conn, msg) u_conn *conn; u_msg *msg;
{
	struct u_user *u = conn->priv;
	char *s, *p;

	p = msg->argv[0];
	while ((s = cut(&p, ",")) != NULL) {
		u_log(LG_FINE, "%s PART %s$%s", u->nick, s, p);

		u_user_part_chan(u, s, msg->argv[1]);
	}
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

static void m_mode(conn, msg) u_conn *conn; u_msg *msg;
{
	int on = 1;
	char *p;
	u_user *tu, *u = conn->priv;
	u_chan *c;
	u_chanuser *cu;

	if (msg->argv[0][0] != '#') {
		tu = u_user_by_nick(msg->argv[0]);
		if (tu == NULL) {
			/* not sure why charybdis does this */
			u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);
		} else if (u != tu) {
			u_user_num(u, ERR_USERSDONTMATCH);
		} else {
			u_user_num(u, ERR_GENERIC, "Can't change user modes yet!");
		}
		return;
	}

	c = u_chan_get(msg->argv[0]);
	if (c == NULL) {
		u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);
		return;
	}

	if (msg->argv[1] == NULL) {
		/* TODO: params or something */
		u_user_num(u, RPL_CHANNELMODEIS, c->name, u_chan_modes(c), "");
		return;
	}

	cu = u_chan_user_find(c, u);
	if (cu == NULL) {
		u_user_num(u, ERR_NOTONCHANNEL, c->name);
		return;
	}
	if (!(cu->flags & CU_PFX_OP)) {
		u_user_num(u, ERR_CHANOPRIVSNEEDED, c->name);
		return;
	}

	ga_argc = msg->argc - 2;
	ga_argv = msg->argv + 2;
	if (ga_argc > 4)
		ga_argc = 4;

	u_chan_m_start();

	for (p=msg->argv[1]; *p; p++) {
		switch (*p) {
		case '+':
		case '-':
			on = *p == '+';
			break;

		default:
			u_chan_mode(c, u, *p, on, getarg);
		}
	}

	p = u_chan_m_end();
	if (*p != '\0') {
		u_sendto_chan(c, NULL, ":%s!%s@%s MODE %s %s", u->nick,
		              u->ident, u->host, c->name, p);
	}
}

u_cmd c_user[] = {
	{ "PING",    CTX_USER, m_ping,    1 },
	{ "PONG",    CTX_USER, m_ping,    0 },
	{ "VERSION", CTX_USER, m_version, 0 },
	{ "MOTD",    CTX_USER, m_motd,    0 },
	{ "PRIVMSG", CTX_USER, m_message, 2 },
	{ "NOTICE",  CTX_USER, m_message, 2 },
	{ "JOIN",    CTX_USER, m_join,    1 },
	{ "PART",    CTX_USER, m_part,    1 },
	{ "TOPIC",   CTX_USER, m_topic,   1 },
	{ "NAMES",   CTX_USER, m_names,   0 },
	{ "MODE",    CTX_USER, m_mode,    1 },
	{ "" },
};

/* ircd-micro, c_user.c -- user commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

int ga_argc = 0;
static char **ga_argv;
static char *getarg()
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

	u_conn_f(conn, ":%S PONG %S :%s", &me, &me, msg->argv[0]);
}

static void m_quit(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user_unlink(conn->priv, msg->argc > 0 ? msg->argv[0] : "Client quit");
	u_conn_close(conn);
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

	u_log(LG_DEBUG, "[%U -> %C] %s", src, tgt, msg->argv[1]);

	u_sendto_chan(tgt, conn, ":%H %s %C :%s", src, msg->command,
	              tgt, msg->argv[1]);
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

	u_sendto_chan(c, NULL, ":%H TOPIC %C :%s", u, c, c->topic);
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
		u_user_num(u, RPL_CHANNELMODEIS, c, u_chan_modes(c), "");
		return;
	}

	cu = u_chan_user_find(c, u);
	if (cu == NULL) {
		u_user_num(u, ERR_NOTONCHANNEL, c);
		return;
	}
	if (!(cu->flags & CU_PFX_OP)) {
		u_user_num(u, ERR_CHANOPRIVSNEEDED, c);
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
	if (*p != '\0')
		u_sendto_chan(c, NULL, ":%H MODE %C %s", u, c, p);
}

struct m_whois_cb_priv {
	u_user *u, *tu;
	char *s, buf[512];
	uint w;
};

static void m_whois_cb(map, c, cu, priv)
u_map *map; u_chan *c; u_chanuser *cu; struct m_whois_cb_priv *priv;
{
	char *p, buf[MAXCHANNAME+3];
	int retrying = 0;

	p = buf;
	if (cu->flags & CU_PFX_OP)
		*p++ = '@';
	if (cu->flags & CU_PFX_VOICE)
		*p++ = '+';
	strcpy(p, c->name);

try_again:
	if (!wrap(priv->buf, &priv->s, priv->w, buf)) {
		if (retrying) {
			u_log(LG_SEVERE, "Can't fit %s into %s!",
			      buf, "RPL_WHOISCHANNELS");
			return;
		}
		u_user_num(priv->u, RPL_WHOISCHANNELS,
			   priv->tu->nick, priv->buf);
		retrying = 1;
		goto try_again;
	}
}

static void m_whois(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *tu, *u = conn->priv;
	u_server *serv;
	char *nick;
	struct m_whois_cb_priv cb_priv;

	/*
	WHOIS aji aji
	:host.irc 311 x aji alex ponychat.net * :Alex Iadicicco
	:host.irc 319 x aji :#chan #foo ...
	*        ***** *   **   = 9
	:host.irc 312 x aji some.host :Host Description
	:host.irc 313 x aji :is a Server Administrator
	:host.irc 671 x aji :is using a secure connection
	:host.irc 317 x aji 1961 1365205045 :seconds idle, signon time
	:host.irc 330 x aji aji :is logged in as
	:host.irc 318 x aji :End of /WHOIS list.
	*/

	nick = strchr(msg->argv[0], ',');
	if (nick != NULL)
		*nick = '\0';
	nick = msg->argv[0];

	tu = u_user_by_nick(nick);

	if (tu == NULL) {
		u_user_num(u, ERR_NOSUCHNICK, nick);
		return;
	}

	if (tu->flags & USER_IS_LOCAL)
		serv = &me;
	else
		serv = USER_REMOTE(tu)->server;

	u_user_num(u, RPL_WHOISUSER, tu->nick, tu->ident, tu->host, tu->gecos);

	cb_priv.u = u;
	cb_priv.tu = tu;
	cb_priv.s = cb_priv.buf;
	cb_priv.w = 512 - (strlen(me.name) + strlen(u->nick) + strlen(tu->nick) + 9);
	u_map_each(tu->channels, m_whois_cb, &cb_priv);
	if (cb_priv.s != cb_priv.buf) /* left over */
		u_user_num(u, RPL_WHOISCHANNELS, tu->nick, cb_priv.buf);

	u_user_num(u, RPL_WHOISSERVER, tu->nick, serv->name, serv->desc);

	if (tu->away[0])
		u_user_num(u, RPL_AWAY, tu->nick, tu->away);

	if (tu->flags & UMODE_OPER)
		u_user_num(u, RPL_WHOISOPERATOR, tu->nick);

	u_user_num(u, RPL_ENDOFWHOIS, tu->nick);
}

static void m_userhost(conn, msg) u_conn *conn; u_msg *msg;
{
	/* USERHOST user1 user2... usern 
	 * :host.irc 302 nick :user1=+~user@host user2=+~user@host ...
	 * *        *****    **   = 8
	 */
	u_user *tu, *u = conn->priv;
	int i, w, rem = 510;
	char buf[512], data[512];
	char *ptr = buf;

	rem -= strlen(me.name) + strlen(u->nick);

	/* TODO - last param could contain multiple targets */
	for (i=0; i<msg->argc; i++) {
		tu = u_user_by_nick(msg->argv[i]);
		if (tu == NULL)
			continue;

		w = sprintf(data, "%s%s=%c%s@%s", tu->nick,
		            ((tu->flags & UMODE_OPER) ? "*" : ""),
		            (tu->away[0] ? '-' : '+'),
		            tu->ident, tu->host);
		if (w + 1 > rem)
			break;

		if (ptr != buf)
			*ptr++ = ' ';

		u_strlcpy(ptr, data, rem);
		ptr += w;
		rem -= w;
	}

	u_user_num(u, RPL_USERHOST, buf);
}

static void m_away(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *u = conn->priv;

	if (msg->argc == 0 || !msg->argv[0][0]) {
		u->away[0] = '\0';
		u_user_num(u, RPL_UNAWAY);
	} else {
		u_strlcpy(u->away, msg->argv[0], MAXAWAY);
		u_user_num(u, RPL_NOWAWAY);
	}
}

/* :serv.irc 352 aji #chan ident my.host serv.irc nick H*@ :hops realname */
static void who_reply(u, tu, c, cu) u_user *u, *tu; u_chan *c; u_chanuser *cu;
{
	u_server *serv;
	char *s, buf[6];
	s = buf;

	if (c != NULL && cu == NULL)
		cu = u_chan_user_find(c, u);
	if (cu == NULL) /* this is an error */
		c = NULL;

	if (tu->flags & USER_IS_LOCAL)
		serv = &me;
	else
		serv = USER_REMOTE(tu)->server;

	*s++ = tu->away[0] ? 'G' : 'H';
	if (tu->flags & UMODE_OPER)
		*s++ = '*';
	if (cu != NULL && (cu->flags & CU_PFX_OP))
		*s++ = '@';
	if (cu != NULL && (cu->flags & CU_PFX_VOICE))
		*s++ = '+';
	*s++ = '\0';

	u_user_num(u, RPL_WHOREPLY, c, tu->ident, tu->host,
	           serv->name, tu->nick, buf, 0, tu->gecos);
}

static void m_who_chan_cb(map, tu, cu, u) u_map *map; u_user *tu, *u; u_chanuser *cu;
{
	who_reply(u, tu, cu->c, cu);
}

static void m_who(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *tu, *u = conn->priv;
	u_chan *c = NULL;
	char *name = msg->argv[0];

	/* TODO: WHOX, operspy? */

	if (name[0] == '#') {
		if ((c = u_chan_get(name)) == NULL)
			goto end;

		u_map_each(c->members, m_who_chan_cb, u);
	} else {
		if ((tu = u_user_by_nick(name)) == NULL)
			goto end;

		/* TODO: chan field */
		who_reply(u, tu, NULL, NULL);
	}

end:
	u_user_num(u, RPL_ENDOFWHO, name);
}

static void m_oper(conn, msg) u_conn *conn; u_msg *msg;
{
	u_conn_num(conn, ERR_NOOPERHOST);
}

static void list_entry(u, c) u_user *u; u_chan *c;
{
	u_user_num(u, RPL_LIST, c->name, c->members->size, c->topic);
}

static void m_list_chan_cb(c, u) u_chan *c; u_user *u;
{
	/* TODO: filtering etc */
	list_entry(u, c);
}

static void m_list(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *u = conn->priv;

	if (msg->argc > 0) {
		u_chan *c = u_chan_get(msg->argv[0]);
		if (c == NULL) {
			u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);
			return;
		}
		u_user_num(u, RPL_LISTSTART);
		list_entry(u, c);
		u_user_num(u, RPL_LISTEND);
		return;
	}

	u_user_num(u, RPL_LISTSTART);
	u_trie_each(all_chans, m_list_chan_cb, u);
	u_user_num(u, RPL_LISTEND);
}

u_cmd c_user[] = {
	{ "PING",    CTX_USER, m_ping,    1 },
	{ "PONG",    CTX_USER, m_ping,    0 },
	{ "QUIT",    CTX_USER, m_quit,    0 },
	{ "VERSION", CTX_USER, m_version, 0 },
	{ "MOTD",    CTX_USER, m_motd,    0 },
	{ "PRIVMSG", CTX_USER, m_message, 2 },
	{ "NOTICE",  CTX_USER, m_message, 2 },
	{ "JOIN",    CTX_USER, m_join,    1 },
	{ "PART",    CTX_USER, m_part,    1 },
	{ "TOPIC",   CTX_USER, m_topic,   1 },
	{ "NAMES",   CTX_USER, m_names,   0 },
	{ "MODE",    CTX_USER, m_mode,    1 },
	{ "WHOIS",   CTX_USER, m_whois,   1 },
	{ "USERHOST",CTX_USER, m_userhost,1 },
	{ "AWAY",    CTX_USER, m_away,    0 },
	{ "WHO",     CTX_USER, m_who,     1 },
	{ "OPER",    CTX_USER, m_oper,    2 },
	{ "LIST",    CTX_USER, m_list,    0 },
	{ "" },
};

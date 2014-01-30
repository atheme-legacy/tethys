/* Tethys, c_user.c -- user commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int m_echo(u_conn *conn, u_msg *msg)
{
	u_user *u = conn->priv;
	char buf[512];
	int i;

	snf(FMT_USER, buf, 512, ":%S NOTICE %U :***", &me, u);

	u_conn_f(conn, "%s Source: %E", buf, msg->src);
	u_conn_f(conn, "%s Command: %s", buf, msg->command);
	u_conn_f(conn, "%s Recieved %d arguments:", buf, msg->argc);

	for (i=0; i<msg->argc; i++)
		u_conn_f(conn, "%s %3d. ^%s$", buf, i, msg->argv[i]);
	return 0;
}

static int m_names(u_conn *conn, u_msg *msg)
{
	u_user *u = conn->priv;
	u_chan *c;

	/* TODO: no arguments version */
	if (msg->argc == 0)
		return 0;

	if (!(c = u_chan_get(msg->argv[0])))
		return u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);

	u_chan_send_names(c, u);
	return 0;
}

static int m_userhost(u_conn *conn, u_msg *msg)
{
	/* USERHOST user1 user2... usern 
	 * :host.irc 302 nick :user1=+~user@host user2=+~user@host ...
	 * *        *****    **   = 8
	 */
	u_user *tu, *u = conn->priv;
	int i, w, max;
	char buf[512], data[512];
	char *ptr = buf;

	max = 501 - strlen(me.name) - strlen(u->nick);
	buf[0] = '\0';

	/* TODO - last param could contain multiple targets */
	for (i=0; i<msg->argc; i++) {
		tu = u_user_by_nick(msg->argv[i]);
		if (tu == NULL)
			continue;

		w = snf(FMT_USER, data, 512, "%s%s=%c%s@%s", tu->nick,
		            ((tu->flags & UMODE_OPER) ? "*" : ""),
		            (tu->away[0] ? '-' : '+'),
		            tu->ident, tu->host);

		if (ptr + w + 1 > buf + max)
			u_user_num(u, RPL_USERHOST, buf);
			ptr = buf;

		if (ptr != buf)
			*ptr++ = ' ';

		u_strlcpy(ptr, data, buf + max - ptr);
		ptr += w;
	}

	if (ptr != buf)
		u_user_num(u, RPL_USERHOST, buf);
	return 0;
}

static int list_entry(u_user *u, u_chan *c)
{
	if ((c->mode & (CMODE_PRIVATE | CMODE_SECRET))
	    && !u_chan_user_find(c, u))
		return 0;
	u_user_num(u, RPL_LIST, c->name, c->members->size, c->topic);
	return 0;
}

static int m_list(u_conn *conn, u_msg *msg)
{
	mowgli_patricia_iteration_state_t state;
	u_user *u = conn->priv;
	u_chan *c;

	if (msg->argc > 0) {
		if (!(c = u_chan_get(msg->argv[0])))
			return u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);
		u_user_num(u, RPL_LISTSTART);
		list_entry(u, c);
		u_user_num(u, RPL_LISTEND);
		return 0;
	}

	u_user_num(u, RPL_LISTSTART);
	MOWGLI_PATRICIA_FOREACH(c, &state, all_chans) {
		if (c->members->size < 3)
			continue;
		list_entry(u, c);
	}
	u_user_num(u, RPL_LISTEND);

	return 0;
}

static void stats_o_cb(u_map *map, char *k, u_oper *o, u_user *u)
{
	char *auth = o->authname[0] ? o->authname : "<any>";
	u_user_num(u, RPL_STATSOLINE, o->name, o->pass, auth);
}

static void stats_i_cb(u_map *map, char *k, u_auth *v, u_user *u)
{
	char buf[CIDR_ADDRSTRLEN];
	u_cidr_to_str(&v->cidr, buf);
	u_user_num(u, RPL_STATSILINE, v->name, v->classname, buf);
}

static int m_stats(u_conn *conn, u_msg *msg)
{
	u_user *u = conn->priv;
	int c, days, hr, min, sec;

	if (!(c = msg->argv[0][0])) /* "STATS :" will do this */
		return u_user_num(u, ERR_NEEDMOREPARAMS, "STATS");

	if (strchr("oi", c) && !(u->flags & UMODE_OPER)) {
		u_user_num(u, ERR_NOPRIVILEGES);
		u_user_num(u, RPL_ENDOFSTATS, c);
		return 0;
	}

	switch (c) {
	case 'o':
		u_map_each(all_opers, (u_map_cb_t*)stats_o_cb, u);
		break;
	case 'i':
		u_map_each(all_auths, (u_map_cb_t*)stats_i_cb, u);
		break;

	case 'u':
		sec = NOW.tv_sec - started;
		min = sec / 60; sec %= 60;
		hr = min / 60; min %= 60;
		days = hr / 24; hr %= 24;

		u_user_num(u, RPL_STATSUPTIME, days, hr, min, sec);
		break;
	}

	u_user_num(u, RPL_ENDOFSTATS, c);
	return 0;
}

static int m_mkpass(u_conn *conn, u_msg *msg)
{
	char buf[CRYPTLEN], salt[CRYPTLEN];

	u_crypto_gen_salt(salt);
	u_crypto_hash(buf, msg->argv[0], salt);

	u_conn_f(conn, ":%S NOTICE %U :%s", &me, conn->priv, buf);
	return 0;
}

u_cmd c_user[] = {
	{ "ECHO",      CTX_USER, m_echo,              0, 0 },
	{ "QUIT",      CTX_USER, m_quit,              0, 0 },
	{ "VERSION",   CTX_USER, m_version,           0, 0 },
	{ "MOTD",      CTX_USER, m_motd,              0, 0 },
	{ "JOIN",      CTX_USER, m_join,              1, 0 },
	{ "PART",      CTX_USER, m_part,              1, 0 },
	{ "TOPIC",     CTX_USER, m_topic,             1, 0 },
	{ "NAMES",     CTX_USER, m_names,             0, 0 },
	{ "MODE",      CTX_USER, m_mode,              1, 0 },
	{ "WHOIS",     CTX_USER, m_whois,             1, 0 },
	{ "USERHOST",  CTX_USER, m_userhost,          1, 0 },
	{ "WHO",       CTX_USER, m_who,               1, 0 },
	{ "OPER",      CTX_USER, m_oper,              2, 0 },
	{ "LIST",      CTX_USER, m_list,              0, 0 },
	{ "NICK",      CTX_USER, m_nick,              1, 0 },
	{ "STATS",     CTX_USER, m_stats,             1, 0 },
	{ "MKPASS",    CTX_USER, m_mkpass,            1, 0 },
	{ "ADMIN",     CTX_USER, m_admin,             0, 0 },
	{ "KILL",      CTX_USER, m_kill,              1, 0 },
	{ "KICK",      CTX_USER, m_kick,              2, 0 },
	{ "SUMMON",    CTX_USER, m_summon,            0, 0 },
	{ "MAP",       CTX_USER, m_map,               0, 0 },
	{ "INVITE",    CTX_USER, m_invite,            2, 0 },

	{ "MODLOAD",   CTX_USER, m_modload,           1, 0 },
	{ "MODUNLOAD", CTX_USER, m_modunload,         1, 0 },
	{ "MODRELOAD", CTX_USER, m_modreload,         1, 0 },
	{ "MODLIST",   CTX_USER, m_modlist,           0, 0 },

	{ "SQUIT",     CTX_USER, not_implemented,     0, 0 },
	{ "LINKS",     CTX_USER, not_implemented,     0, 0 },
	{ "TIME",      CTX_USER, not_implemented,     0, 0 },
	{ "CONNECT",   CTX_USER, not_implemented,     0, 0 },
	{ "TRACE",     CTX_USER, not_implemented,     0, 0 },
	{ "INFO",      CTX_USER, not_implemented,     0, 0 },
	{ "WHOWAS",    CTX_USER, not_implemented,     0, 0 },
	{ "REHASH",    CTX_USER, not_implemented,     0, 0 },
	{ "RESTART",   CTX_USER, not_implemented,     0, 0 },
	{ "USERS",     CTX_USER, not_implemented,     0, 0 },
	{ "OPERWALL",  CTX_USER, not_implemented,     0, 0 },
	{ "ISON",      CTX_USER, not_implemented,     0, 0 },

	{ "" },
};

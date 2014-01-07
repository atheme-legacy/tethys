/* ircd-micro, c_user.c -- user commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int not_implemented(u_conn *conn, u_msg *msg)
{
	u_conn_f(conn, ":%S NOTICE %U :*** %s is not yet implemented!",
	         &me, conn->priv, msg->command);
	return 0;
}

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

static int m_quit(u_conn *conn, u_msg *msg)
{
	char *r = msg->argc > 0 ? msg->argv[0] : "Client quit";
	u_user *u = conn->priv;

	u_sendto_visible(u, ST_USERS, ":%H QUIT :Quit: %s", u, r);
	u_conn_f(conn, ":%H QUIT :Quit: %s", u, r);
	u_roster_f(R_SERVERS, NULL, ":%H QUIT :Quit: %s", u, r);

	u_user_unlink(u);
	u_conn_close(conn);

	return 0;
}

static int m_version(u_conn *conn, u_msg *msg)
{
	u_user *u = conn->priv;
	u_user_num(u, RPL_VERSION, PACKAGE_FULLNAME, me.name,
	           PACKAGE_COPYRIGHT);
	u_user_send_isupport(USER_LOCAL(u));
	return 0;
}

static int m_motd(u_conn *conn, u_msg *msg)
{
	u_user *u = conn->priv;
	u_user_send_motd(USER_LOCAL(u));
	return 0;
}

static int try_join_chan(u_user_local *ul, char *chan, char *key)
{
	u_conn *conn = ul->conn;
	u_user *u = USER(ul);
	u_chan *c, *fwd;
	u_chanuser *cu;
	int num;
	char *modes;

	c = u_chan_get_or_create(chan);

	if (c == NULL) {
		u_user_num(u, ERR_GENERIC, "Can't get or create channel!");
		return 0;
	}

	cu = u_chan_user_find(c, u);
	if (cu != NULL)
		return 0;

	num = u_entry_blocked(c, u, key);
	if (num != 0) {
		fwd = u_find_forward(c, u, key);
		if (fwd == NULL || u_chan_user_find(fwd, u)) {
			u_user_num(u, num, c);
			return 0;
		}
		c = fwd;
	}

	cu = u_chan_user_add(c, u);
	u_del_invite(c, u);
	u_sendto_chan(c, NULL, ST_USERS, ":%H JOIN %C", u, c);

	if (c->members->size == 1) {
		modes = u_chan_modes(c, 1);

		u_log(LG_VERBOSE, "Channel %C %s created by %U", c, modes, u);
		cu->flags |= CU_PFX_OP;

		u_roster_f(R_SERVERS, NULL, ":%S SJOIN %u %C %s :@%U",
		           &me, c->ts, c, modes, u);
		u_conn_f(conn, ":%S MODE %C %s", &me, c, modes);
	} else {
		u_log(LG_DEBUG, "%U (local) join %C", u, c);
		u_roster_f(R_SERVERS, NULL, ":%U JOIN %u %C +", u, c->ts, c);
	}

	u_chan_send_topic(c, u);
	u_chan_send_names(c, u);

	return 0;
}

static int m_join(u_conn *conn, u_msg *msg)
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

		try_join_chan(USER_LOCAL(u), s, *keys_p++);
	}

	return 0;
}

static int m_part(u_conn *conn, u_msg *msg)
{
	char *q, chans[512], buf[512];
	u_user *u = conn->priv;
	u_chan *c;
	u_chanuser *cu;
	char *s, *p;

	buf[0] = '\0';
	if (msg->argv[1])
		sprintf(buf, " :%s", msg->argv[1]);

	q = chans;

	p = msg->argv[0];
	while ((s = cut(&p, ",")) != NULL) {
		u_log(LG_FINE, "%s PART %s$%s", u->nick, s, p);

		if (!(c = u_chan_get(s))) {
			u_user_num(u, ERR_NOSUCHCHANNEL, s);
			continue;
		}

		if (!(cu = u_chan_user_find(c, u))) {
			u_user_num(u, ERR_NOTONCHANNEL, c);
			continue;
		}

		u_sendto_chan(c, NULL, ST_USERS, ":%H PART %C%s", u, c, buf);
		u_chan_user_del(cu);

		q += sprintf(q, "%s%s", q==chans?"":",", c->name);

		if (c->members->size == 0) {
			u_log(LG_DEBUG, "Dropping channel %C", c);
			u_chan_drop(c);
		}
	}

	if (q != chans) {
		u_log(LG_DEBUG, "%U parted from %s%s", u, chans, buf);
		u_roster_f(R_SERVERS, conn, ":%U PART %s%s", u, chans, buf);
	}

	return 0;
}

static int m_topic(u_conn *conn, u_msg *msg)
{
	u_user *u = conn->priv;
	u_chan *c;
	u_chanuser *cu;

	if (!(c = u_chan_get(msg->argv[0])))
		return u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);
	if (msg->argc == 1)
		return u_chan_send_topic(c, u);
	if (!(cu = u_chan_user_find(c, u)))
		return u_user_num(u, ERR_NOTONCHANNEL, c);
	if ((c->mode & CMODE_TOPIC) && !(cu->flags & CU_PFX_OP))
		return u_user_num(u, ERR_CHANOPRIVSNEEDED, c);

	u_strlcpy(c->topic, msg->argv[1], MAXTOPICLEN+1);
	u_strlcpy(c->topic_setter, u->nick, MAXNICKLEN+1);
	c->topic_time = NOW.tv_sec;

	u_sendto_chan(c, NULL, ST_ALL, ":%H TOPIC %C :%s", u, c, c->topic);

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

static int mode_user(u_user *u, char *s)
{
	int on = 1;

	u_user_m_start(u);

	for (; *s; s++) {
		switch (*s) {
		case '+':
		case '-':
			on = (*s == '+');
			break;

		default:
			u_user_mode(u, *s, on);
		}
	}

	u_user_m_end(u);

	return 0;
}

static int m_mode(u_conn *conn, u_msg *msg)
{
	int parc;
	char **parv;
	u_user *tu, *u = conn->priv;
	u_chan *c;
	u_modes m;
	u_mode_info *info;

	if (msg->argv[0][0] != '#') {
		tu = u_user_by_nick(msg->argv[0]);
		if (tu == NULL) {
			/* not sure why charybdis does this */
			u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);
		} else if (u != tu) {
			u_user_num(u, ERR_USERSDONTMATCH);
		} else {
			return mode_user(u, msg->argv[1]);
		}
		return 0;
	}

	c = u_chan_get(msg->argv[0]);
	if (!(c = u_chan_get(msg->argv[0])))
		return u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);

	m.perms = u_chan_user_find(c, u);

	if (msg->argv[1] == NULL) {
		u_user_num(u, RPL_CHANNELMODEIS, c, u_chan_modes(c, !!m.perms));
		return 0;
	}

	parc = msg->argc - 1;
	parv = msg->argv + 1;
	if (parc > 5)
		parc = 5;

	m.setter = u;
	m.target = c;
	m.flags = 0;

	u_mode_process(&m, cmodes, parc, parv);

	if (m.u.buf[0] || m.u.data[0]) {
		u_sendto_chan(c, NULL, ST_USERS,
		              ":%H MODE %C %s%s", u, c, m.u.buf, m.u.data);
	}
	if (m.s.buf[0] || m.s.data[0]) {
		u_roster_f(R_SERVERS, NULL, ":%U TMODE %u %C %s%s",
		           u, c->ts, c, m.s.buf, m.s.data);
	}

	for (info=cmodes; info->ch; info++) {
		if (!strchr(m.list, info->ch))
			continue;
		u_chan_send_list(c, u, memberp(c, info->data));
	}

	if (m.flags & CM_DENY)
		u_user_num(u, ERR_CHANOPRIVSNEEDED, c);
	return 0;
}

struct m_whois_cb_priv {
	u_user *u, *tu;
	char *s, buf[512];
	uint w;
};

static void m_whois_cb(u_map *map, u_chan *c, u_chanuser *cu, struct m_whois_cb_priv *priv)
{
	char *p, buf[MAXCHANNAME+3];
	int retrying = 0;

	if ((c->mode & (CMODE_PRIVATE | CMODE_SECRET))
	    && !u_chan_user_find(c, priv->u))
		return;

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

static int m_whois(u_conn *conn, u_msg *msg)
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

	if (!(tu = u_user_by_nick(nick)))
		return u_user_num(u, ERR_NOSUCHNICK, nick);

	serv = u_user_server(tu);

	u_user_num(u, RPL_WHOISUSER, tu->nick, tu->ident, tu->host, tu->gecos);

	cb_priv.u = u;
	cb_priv.tu = tu;
	cb_priv.s = cb_priv.buf;
	cb_priv.w = 512 - (strlen(me.name) + strlen(u->nick) + strlen(tu->nick) + 9);
	u_map_each(tu->channels, (u_map_cb_t*)m_whois_cb, &cb_priv);
	if (cb_priv.s != cb_priv.buf) /* left over */
		u_user_num(u, RPL_WHOISCHANNELS, tu->nick, cb_priv.buf);

	u_user_num(u, RPL_WHOISSERVER, tu->nick, serv->name, serv->desc);

	if (tu->away[0])
		u_user_num(u, RPL_AWAY, tu->nick, tu->away);

	if (tu->flags & UMODE_OPER)
		u_user_num(u, RPL_WHOISOPERATOR, tu->nick);

	u_user_num(u, RPL_ENDOFWHOIS, tu->nick);
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

/* :serv.irc 352 aji #chan ident my.host serv.irc nick H*@ :hops realname */
static void who_reply(u_user *u, u_user *tu, u_chan *c, u_chanuser *cu)
{
	u_server *serv;
	char *s, buf[6];
	s = buf;

	if (c != NULL && cu == NULL)
		cu = u_chan_user_find(c, u);
	if (cu == NULL) /* this is an error */
		c = NULL;

	serv = u_user_server(tu);

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

static void m_who_chan_cb(u_map *map, u_user *tu, u_chanuser *cu, u_user *u)
{
	who_reply(u, tu, cu->c, cu);
}

static int m_who(u_conn *conn, u_msg *msg)
{
	u_user *tu, *u = conn->priv;
	u_chan *c = NULL;
	char *name = msg->argv[0];

	/* TODO: WHOX, operspy? */

	if (name[0] == '#') {
		if ((c = u_chan_get(name)) == NULL)
			goto end;

		u_map_each(c->members, (u_map_cb_t*)m_who_chan_cb, u);
	} else {
		if ((tu = u_user_by_nick(name)) == NULL)
			goto end;

		/* TODO: chan field */
		who_reply(u, tu, NULL, NULL);
	}

end:
	u_user_num(u, RPL_ENDOFWHO, name);
	return 0;
}

static int m_oper(u_conn *conn, u_msg *msg)
{
	u_user_local *ul = conn->priv;
	u_user *u = USER(ul);
	u_oper *oper;

	if (!(oper = u_find_oper(conn->auth, msg->argv[0], msg->argv[1])))
		return u_conn_num(conn, ERR_NOOPERHOST);

	ul->oper = oper;
	u->flags |= UMODE_OPER;
	u_conn_f(conn, ":%U MODE %U +o", u, u);
	u_conn_num(conn, RPL_YOUREOPER);

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

static int m_nick(u_conn *conn, u_msg *msg)
{
	u_user *u = conn->priv;
	char *s, *newnick = msg->argv[0];

	/* cut newnick to nicklen */
	if (strlen(newnick) > MAXNICKLEN)
		newnick[MAXNICKLEN] = '\0';

	if (!is_valid_nick(newnick))
		return u_user_num(u, ERR_ERRONEOUSNICKNAME, newnick);

	/* due to the scandalous origins, (~ being uppercase of ^) and ~
	 * being disallowed as a nick char, we need to chop the first ~
	 * instead of just erroring.
	 */ 
	if ((s = strchr(newnick, '~')))
		*s = '\0';

	/* Check for case change */
	if (irccmp(u->nick, newnick) && u_user_by_nick(newnick))
		return u_user_num(u, ERR_NICKNAMEINUSE, newnick);

	/* ignore changes to the exact same nick */
	if (streq(u->nick, newnick))
		return 0;

	/* Send these BEFORE clobbered --Elizabeth */
	u_sendto_visible(u, ST_USERS, ":%H NICK :%s", u, newnick);
	u_roster_f(R_SERVERS, NULL, ":%H NICK %s %u", u, newnick, NOW.tv_sec);
	u_conn_f(conn, ":%H NICK :%s", u, newnick);

	u_user_set_nick(u, newnick, NOW.tv_sec);

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

static int m_admin(u_conn *conn, u_msg *msg)
{
	u_conn_num(conn, RPL_ADMINME, &me);
	u_conn_num(conn, RPL_ADMINLOC1, my_admin_loc1);
	u_conn_num(conn, RPL_ADMINLOC2, my_admin_loc2);
	u_conn_num(conn, RPL_ADMINEMAIL, my_admin_email);
	return 0;
}

static int m_kill(u_conn *conn, u_msg *msg)
{
	u_user *tu, *u = conn->priv;
	char *reason = msg->argv[1] ? msg->argv[1] : "<No reason given>";
	char buf[512];

	if (!(u->flags & UMODE_OPER))
		return u_user_num(u, ERR_NOPRIVILEGES);
	if (!(tu = u_user_by_nick(msg->argv[0])))
		return u_user_num(u, ERR_NOSUCHNICK, msg->argv[0]);

	snf(FMT_USER, buf, 512, "%U (%s)", u, reason);

	u_sendto_visible(tu, ST_USERS, ":%H QUIT :Killed (%s)", tu, buf);
	u_roster_f(R_SERVERS, NULL, ":%U KILL %U :%s", u, tu, me.name, buf);

	if (IS_LOCAL_USER(tu)) {
		u_conn_f(USER_LOCAL(tu)->conn, ":%H QUIT :Killed (%s)", tu, buf);
		u_conn_close(USER_LOCAL(tu)->conn);
	}
	u_user_unlink(tu);

	return 0;
}

static int m_kick(u_conn *conn, u_msg *msg)
{
	u_user *tu, *u = conn->priv;
	u_chan *c;
	u_chanuser *tcu, *cu;
	char *r = msg->argv[2];

	if (!(c = u_chan_get(msg->argv[0])))
		return u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[0]);
	if (!(tu = u_user_by_nick(msg->argv[1])))
		return u_user_num(u, ERR_NOSUCHNICK, msg->argv[1]);
	if (!(cu = u_chan_user_find(c, u)))
		return u_user_num(u, ERR_NOTONCHANNEL, c);
	if (!(tcu = u_chan_user_find(c, tu)))
		return u_user_num(u, ERR_USERNOTINCHANNEL, tu, c);
	if (!(cu->flags & CU_PFX_OP))
		return u_user_num(u, ERR_CHANOPRIVSNEEDED, c);

	u_log(LG_FINE, "%U KICK %U from %C (reason=%s)", u, tu, c, r);
	r = r ? r : tu->nick;
	u_sendto_chan(c, NULL, ST_USERS, ":%H KICK %C %U :%s", u, c, tu, r);
	u_roster_f(R_SERVERS, NULL, ":%H KICK %C %U :%s", u, c, tu, r);

	u_chan_user_del(tcu);

	return 0;
}

static int m_summon(u_conn *conn, u_msg *msg)
{
	u_conn_num(conn, ERR_SUMMONDISABLED);
	return 0;
}

struct map_priv {
	char indent[512], buf[512];
	u_user *u;

	u_server *sv;
	int depth, left;
};
static void do_map(struct map_priv *p)
{
	int len, left, depth = p->depth << 2;
	mowgli_patricia_iteration_state_t state;
	u_server *tsv, *sv = p->sv;

	p->indent[depth] = '\0';
	if (depth != 0) {
		p->left--;
		p->indent[depth - 3] = p->left ? '|' : '`';
		p->indent[depth - 2] = '-';
	}

	len = snf(FMT_USER, p->buf, 512, "%s%s[%s] ",
	          p->indent, sv->name, sv->sid);
	memset(p->buf + len, '-', 512 - len);
	snf(FMT_USER, p->buf + 50, 462, " | Users: %5d", sv->nusers);

	/* send! */
	u_user_num(p->u, RPL_MAP, p->buf);

	p->indent[depth] = ' ';

	if (sv->nlinks > 0) {
		left = p->left;
		p->left = sv->nlinks;
		p->depth = (depth >> 2) + 1;
		MOWGLI_PATRICIA_FOREACH(tsv, &state, servers_by_sid) {
			if (tsv->parent != sv)
				continue;
			p->sv = tsv;
			do_map(p);
			p->sv = sv;
		}
		p->depth = (depth >> 2);
		p->left = left;
	}

	if (depth != 0) {
		p->indent[depth - 3] = ' ';
		p->indent[depth - 2] = ' ';
	}
}

static int m_map(u_conn *conn, u_msg *msg)
{
	struct map_priv p;
	u_user *u = conn->priv;

	if (!(u->flags & UMODE_OPER))
		return u_user_num(u, ERR_NOPRIVILEGES);

	memset(p.indent, ' ', 512);
	p.sv = &me;
	p.depth = 0;
	p.u = u;
	do_map(&p);

	u_user_num(u, RPL_MAPEND);
	return 0;
}

static int m_invite(u_conn *conn, u_msg *msg)
{
	u_entity te;
	u_user *tu, *u = conn->priv;
	u_chan *c;
	u_chanuser *cu;

	if (!(tu = u_user_by_nick(msg->argv[0])))
		return u_user_num(u, ERR_NOSUCHNICK, msg->argv[0]);
	if (!(c = u_chan_get(msg->argv[1])))
		return u_user_num(u, ERR_NOSUCHCHANNEL, msg->argv[1]);
	if (!(cu = u_chan_user_find(c, u)))
		return u_user_num(u, ERR_NOTONCHANNEL, c);
	if (u_chan_user_find(c, tu))
		return u_user_num(u, ERR_USERONCHANNEL, tu, c);
	if (!(cu->flags & CU_PFX_OP) && !(c->mode & CMODE_FREEINVITE))
		return u_user_num(u, ERR_CHANOPRIVSNEEDED, c);

	u_entity_from_user(&te, tu);
	if (ENT_IS_LOCAL(&te)) {
		u_add_invite(c, tu);
		u_conn_f(te.link, ":%H INVITE %U :%C", u, tu, c);
	} else {
		u_conn_f(te.link, ":%H INVITE %U %C :%u", u, tu, c, c->ts);
	}

	/* TODO: who sees RPL_INVITING? */
	u_user_num(u, RPL_INVITING, tu, c);
	u_log(LG_VERBOSE, "Local %U invited %U to %C", u, tu, c);

	return 0;
}

u_cmd c_user[] = {
	{ "ECHO",      CTX_USER, m_echo,    0 },
	{ "QUIT",      CTX_USER, m_quit,    0 },
	{ "VERSION",   CTX_USER, m_version, 0 },
	{ "MOTD",      CTX_USER, m_motd,    0 },
	{ "JOIN",      CTX_USER, m_join,    1 },
	{ "PART",      CTX_USER, m_part,    1 },
	{ "TOPIC",     CTX_USER, m_topic,   1 },
	{ "NAMES",     CTX_USER, m_names,   0 },
	{ "MODE",      CTX_USER, m_mode,    1 },
	{ "WHOIS",     CTX_USER, m_whois,   1 },
	{ "USERHOST",  CTX_USER, m_userhost,1 },
	{ "WHO",       CTX_USER, m_who,     1 },
	{ "OPER",      CTX_USER, m_oper,    2 },
	{ "LIST",      CTX_USER, m_list,    0 },
	{ "NICK",      CTX_USER, m_nick,    1 },
	{ "STATS",     CTX_USER, m_stats,   1 },
	{ "MKPASS",    CTX_USER, m_mkpass,  1 },
	{ "ADMIN",     CTX_USER, m_admin,   0 },
	{ "KILL",      CTX_USER, m_kill,    1 },
	{ "KICK",      CTX_USER, m_kick,    2 },
	{ "SUMMON",    CTX_USER, m_summon,  0 },
	{ "MAP",       CTX_USER, m_map,     0 },
	{ "INVITE",    CTX_USER, m_invite,  2 },

	{ "SQUIT",     CTX_USER, not_implemented, 0 },
	{ "LINKS",     CTX_USER, not_implemented, 0 },
	{ "TIME",      CTX_USER, not_implemented, 0 },
	{ "CONNECT",   CTX_USER, not_implemented, 0 },
	{ "TRACE",     CTX_USER, not_implemented, 0 },
	{ "INFO",      CTX_USER, not_implemented, 0 },
	{ "WHOWAS",    CTX_USER, not_implemented, 0 },
	{ "REHASH",    CTX_USER, not_implemented, 0 },
	{ "RESTART",   CTX_USER, not_implemented, 0 },
	{ "USERS",     CTX_USER, not_implemented, 0 },
	{ "OPERWALL",  CTX_USER, not_implemented, 0 },
	{ "ISON",      CTX_USER, not_implemented, 0 },

	{ "" },
};

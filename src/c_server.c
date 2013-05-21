/* ircd-micro, c_server.c -- server commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int not_implemented(conn, msg) u_conn *conn; u_msg *msg;
{
	u_log(LG_SEVERE, "%S used unimplemented S2S command %s",
	      conn->priv, msg->command);
	return 0;
}

static int idfk(conn, msg) u_conn *conn; u_msg *msg;
{
	u_log(LG_WARN, "%S sent bad %s, it seems", conn->priv, msg->command);
	return 0;
}

static int m_error(conn, msg) u_conn *conn; u_msg *msg;
{
	u_log(LG_ERROR, "%S is closing connection via ERROR (%s)", conn->priv,
	      msg->argc > 0 ? msg->argv[0] : "no message");
	u_conn_error(conn, "Peer sent ERROR!");
	return 0;
}

static int m_svinfo(conn, msg) u_conn *conn; u_msg *msg;
{
	int tsdelta;
	u_server *sv = conn->priv;

	if (!(sv->flags & SERVER_IS_BURSTING)) {
		u_log(LG_ERROR, "%S tried to send SVINFO again!", sv);
		return 0;
	}

	if (atoi(msg->argv[0]) < 6) {
		u_conn_error(conn, "Max TS version is not 6!");
		return 0;
	}

	tsdelta = atoi(msg->argv[3]) - (int)NOW.tv_sec;
	if (tsdelta < 0)
		tsdelta = -tsdelta;

	if (tsdelta > 10)
		u_log(LG_WARN, "%S has TS delta of %d", conn->priv, tsdelta);
	if (tsdelta > 60) {
		u_log(LG_ERROR, "%S has excessive TS delta, killing", conn->priv);
		u_conn_error(conn, "Excessive TS delta");
		return 0;
	}

	return 0;
}

static u_user *uid_generic(conn, msg) u_conn *conn; u_msg *msg;
{
	u_server *sv;
	u_user_remote *ur;
	u_user *u;
	char buf[512];

	if (!(sv = u_server_by_sid(msg->srcstr)))
		return NULL;

	/* TODO: check for collision! */

	ur = u_user_new_remote(sv, msg->argv[7]);
	u = USER(ur);

	u_user_set_nick(ur, msg->argv[0], atoi(msg->argv[2]));

	/* TODO: set umodes */

	u_strlcpy(u->ident, msg->argv[4], MAXIDENT+1);
	u_strlcpy(u->host, msg->argv[5], MAXHOST+1);
	u_strlcpy(u->ip, msg->argv[6], INET_ADDRSTRLEN);
	u_strlcpy(u->gecos, msg->argv[msg->argc - 1], MAXGECOS+1);

	/* servers are required to have EUID */
	u_user_make_euid(u, buf);
	u_roster_f(R_SERVERS, conn, "%s", buf);

	return u;
}

static int m_euid(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *u;

	if (!(u = uid_generic(conn, msg)))
		return idfk(conn, msg);

	u_strlcpy(u->realhost, msg->argv[8], MAXHOST+1);
	if (msg->argv[9][0] != '*')
		u_strlcpy(u->acct, msg->argv[9], MAXACCOUNT+1);
	return 0;
}

static int m_uid(conn, msg) u_conn *conn; u_msg *msg;
{
	if (!uid_generic(conn, msg))
		return idfk(conn, msg);
	return 0;
}

static int m_sjoin(conn, msg) u_conn *conn; u_msg *msg;
{
	u_chan *c;
	u_user *u;
	u_chanuser *cu;
	char *p, *s;
	char *m, mbuf[16];
	uint flags;

	if (!ENT_IS_SERVER(msg->src))
		return u_log(LG_ERROR, "Got SJOIN from non-server, wtf");
	if (!(c = u_chan_get_or_create(msg->argv[1])))
		return u_log(LG_ERROR, "Could not get/create %s for SJOIN!",
		             msg->argv[1]);

	/* TODO: check TS */
	/* TODO: apply channel modes?! */

	p = msg->argv[msg->argc-1];
	while ((s = cut(&p, " "))) {
		/* TODO: verify user is behind source! */

		m = mbuf;
		for (flags=0; !isdigit(*s); s++) {
			if (*s == '@') {
				*m++ = 'o';
				flags |= CU_PFX_OP;
			} else if (*s == '+') {
				*m++ = 'v';
				flags |= CU_PFX_VOICE;
			}
		}
		*m++ = '\0';

		if (!(u = u_user_by_uid(s))) {
			u_log(LG_ERROR, "%E tried to SJOIN nonexistent %s!",
			      msg->src, s);
			continue;
		}

		cu = u_chan_user_add(c, u);
		cu->flags = flags;

		u_sendto_chan(c, NULL, ST_USERS, ":%H JOIN :%C", u, c);
		if (flags != 0) {
			u_sendto_chan(c, NULL, ST_USERS,
			              ":%E MODE %C +%s %s%s%s",
			              msg->src, c, mbuf, u->nick,
			              mbuf[1] ? " " : "",
			              mbuf[1] ? u->nick : "");
		}
	}

	u_roster_f(R_SERVERS, conn, ":%E SJOIN %s %s %s :%s",
	           msg->src, msg->argv[0], msg->argv[1],
	           msg->argv[2], msg->argv[3]);
	return 0;
}

static int m_join(conn, msg) u_conn *conn; u_msg *msg;
{
	u_chan *c;
	u_chanuser *cu;

	if (!msg->src || !ENT_IS_USER(msg->src)) {
		return u_log(LG_ERROR, "Can't use JOIN source %s from %G",
		             msg->srcstr, conn);
	}

	if (!(c = u_chan_get(msg->argv[1]))) {
		return u_log(LG_ERROR, "%G tried to JOIN %E to nonexistent chan %s",
		             conn, msg->src, msg->argv[1]);
	}

	/* TODO: check TS */

	cu = u_chan_user_add(c, msg->src->v.u);
	u_log(LG_DEBUG, "%U (remote) join %C", cu->u, c);

	u_sendto_chan(c, NULL, ST_USERS, ":%H JOIN %C", cu->u, c);
	u_roster_f(R_SERVERS, conn, ":%U JOIN %s %C +", cu->u, msg->argv[0], c);

	return 0;
}

static int m_tmode(conn, msg) u_conn *conn; u_msg *msg;
{
	int parc;
	char **parv;
	u_chan *c;
	u_modes m;

	if (!msg->src) {
		u_entity_from_server(msg->src, &me);
		u_log(LG_WARN, "Can't use TMODE source %s from %G, using %E.",
		             msg->srcstr, conn, msg->src);
	}

	if (!(c = u_chan_get(msg->argv[1]))) {
		return u_log(LG_ERROR, "%G tried to TMODE nonexistent chan %s",
		             conn, msg->argv[1]);
	}

	/* TODO: check TS */

	parc = msg->argc - 2;
	parv = msg->argv + 2;

	m.setter = NULL;
	if (ENT_IS_USER(msg->src))
		m.setter = msg->src->v.u;
	m.target = c;
	m.perms = &me;
	m.flags = 0;

	u_mode_process(&m, cmodes, parc, parv);

	if (m.u.buf[0] || m.u.data[0]) {
		if (ENT_IS_USER(msg->src)) {
			u_sendto_chan(c, NULL, ST_USERS,
				      ":%H MODE %C %s%s", msg->src->v.u, c,
			              m.u.buf, m.u.data);
		} else {
			u_sendto_chan(c, NULL, ST_USERS,
				      ":%E MODE %C %s%s", msg->src, c,
			              m.u.buf, m.u.data);
		}
	}
	if (m.s.buf[0] || m.s.data[0]) {
		u_roster_f(R_SERVERS, conn, ":%E TMODE %u %C %s%s",
		           msg->src, c->ts, c, m.s.buf, m.s.data);
	}

	return 0;
}

static int m_kill(conn, msg) u_conn *conn; u_msg *msg;
{
	char *r, buf[512];
	u_user *u;

	if (!msg->src) {
		u_entity_from_server(msg->src, &me);
		u_log(LG_WARN, "Can't use KILL source %s from %G, using %E.",
		      msg->srcstr, conn, msg->src);
	}

	if (!(u = u_user_by_uid(msg->argv[0]))) {
		return u_log(LG_ERROR, "%G tried to KILL nonexistent user %s",
		             conn, msg->argv[0]);
	}

	r = "<No reason given>";
	buf[0] = '\0';
	if (msg->argc > 1) {
		r = msg->argv[1];
		sprintf(buf, " :%s", msg->argv[1]);
	}

	if (IS_LOCAL_USER(u)) {
		u_user_local *ul = USER_LOCAL(u);
		u_conn_f(ul->conn, ":%H QUIT :Killed (%s)", u, r);
		u_conn_close(ul->conn);
	}

	u_sendto_visible(u, ST_USERS, ":%H QUIT :Killed (%s)", u, r);
	u_roster_f(R_SERVERS, conn, ":%E KILL %U%s", msg->src, u, buf);

	u_user_unlink(u);

	return 0;
}

static int m_quit(conn, msg) u_conn *conn; u_msg *msg;
{
	char buf[512];
	u_user *u;

	if (!msg->src || !ENT_IS_USER(msg->src)) {
		return u_log(LG_WARN, "Can't use QUIT source %s from %G!",
		             msg->srcstr, conn);
	}

	u = msg->src->v.u;

	buf[0] = '\0';
	if (msg->argc > 0)
		sprintf(buf, " :%s", msg->argv[0]);

	if (IS_LOCAL_USER(u)) { /* possible? */
		u_user_local *ul = USER_LOCAL(ul);
		u_log(LG_WARN, "%G sent QUIT for my user %U", conn, u);
		u_conn_f(ul->conn, ":%H QUIT%s", u, buf);
		u_conn_close(ul->conn);
	}

	u_sendto_visible(u, ST_USERS, ":%H QUIT%s", u, buf);
	u_roster_f(R_SERVERS, conn, ":%H QUIT%s", u, buf);

	u_user_unlink(u);

	return 0;
}

static int m_sid(conn, msg) u_conn *conn; u_msg *msg;
{
	if (!ENT_IS_SERVER(msg->src)) {
		return u_log(LG_WARN, "Can't use SID source %s from %G!",
		             msg->srcstr, conn);
	}

	u_server_new_remote(msg->src->v.sv,
	                    msg->argv[2], /* sid */
	                    msg->argv[0], /* name */
	                    msg->argv[3]); /* description */
	u_roster_f(R_SERVERS, conn, ":%E SID %s %s %s :%s",
	           msg->src, msg->argv[0], msg->argv[1],
	           msg->argv[2], msg->argv[3]);

	return 0;
}

static int m_part(conn, msg) u_conn *conn; u_msg *msg;
{
	u_chan *c;
	u_user *u;
	
	if (!(c = u_chan_get(msg->argv[0]))) {
		return u_log(LG_ERROR, "%G tried to PART nonexistent chan %s",
		             conn, msg->argv[0]);
	}
	
	u = msg->src->v.u;

	char *s, *p;

	p = msg->argv[0];
	while ((s = cut(&p, ",")) != NULL) {
		u_log(LG_FINE, "%s PART %s$%s", u->nick, s, p);

		u_user_part_chan(u, s, msg->argv[1]);
	}

	return 0;
}

u_cmd c_server[] = {
	{ "ERROR",       CTX_SERVER, m_error,         0 },
	{ "SVINFO",      CTX_SERVER, m_svinfo,        4 },

	{ "EUID",        CTX_SERVER, m_euid,         11 },
	{ "UID",         CTX_SERVER, m_uid,           9 },

	{ "SJOIN",       CTX_SERVER, m_sjoin,         4 },
	{ "JOIN",        CTX_SERVER, m_join,          3 },

	{ "TMODE",       CTX_SERVER, m_tmode,         3 },

	{ "KILL",        CTX_SERVER, m_kill,          1 },
	{ "QUIT",        CTX_SERVER, m_quit,          0 },

	{ "SID",         CTX_SERVER, m_sid,           4 },

	{ "ADMIN",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "AWAY",        CTX_SERVER, not_implemented, 0 },
	{ "BAN",         CTX_SERVER, not_implemented, 0 },
	{ "BMASK",       CTX_SERVER, not_implemented, 0 },
	{ "CHGHOST",     CTX_SERVER, not_implemented, 0 },
	{ "CONNECT",     CTX_SERVER, not_implemented, 0 },
	{ "ENCAP",       CTX_SERVER, not_implemented, 0 },
	{ "GLINE",       CTX_SERVER, not_implemented, 0 },
	{ "GUNGLINE",    CTX_SERVER, not_implemented, 0 },
	{ "INFO",        CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "INVITE",      CTX_SERVER, not_implemented, 0 },
	{ "JUPE",        CTX_SERVER, not_implemented, 0 },
	{ "KICK",        CTX_SERVER, not_implemented, 0 },
	{ "KLINE",       CTX_SERVER, not_implemented, 0 },
	{ "KNOCK",       CTX_SERVER, not_implemented, 0 },
	{ "LINKS",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "LOCOPS",      CTX_SERVER, not_implemented, 0 },
	{ "LUSERS",      CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "MLOCK",       CTX_SERVER, not_implemented, 0 },
	{ "MODE",        CTX_SERVER, not_implemented, 0 },
	{ "MOTD",        CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "NICK",        CTX_SERVER, not_implemented, 0 },
	{ "NICKDELAY",   CTX_SERVER, not_implemented, 0 },
	{ "OPERWALL",    CTX_SERVER, not_implemented, 0 },
	{ "PART",        CTX_SERVER, m_part			, 0 },
	{ "PRIVS",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "RESV",        CTX_SERVER, not_implemented, 0 },
	{ "SAVE",        CTX_SERVER, not_implemented, 0 },
	{ "SERVER",      CTX_SERVER, not_implemented, 0 },
	{ "SIGNON",      CTX_SERVER, not_implemented, 0 },
	{ "SQUIT",       CTX_SERVER, not_implemented, 0 },
	{ "STATS",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "TB",          CTX_SERVER, not_implemented, 0 },
	{ "TIME",        CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "TOPIC",       CTX_SERVER, not_implemented, 0 },
	{ "TRACE",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "UNKLINE",     CTX_SERVER, not_implemented, 0 },
	{ "UNRESV",      CTX_SERVER, not_implemented, 0 },
	{ "UNXLINE",     CTX_SERVER, not_implemented, 0 },
	{ "USERS",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "VERSION",     CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "WALLOPS",     CTX_SERVER, not_implemented, 0 },
	{ "WHOIS",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "XLINE",       CTX_SERVER, not_implemented, 0 },
	{ "" }
};

/* ircd-micro, c_server.c -- server commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void not_implemented(conn, msg) u_conn *conn; u_msg *msg;
{
	u_log(LG_SEVERE, "%S used unimplemented S2S command %s",
	      conn->priv, msg->command);
}

static void idfk(conn, msg) u_conn *conn; u_msg *msg;
{
	u_log(LG_WARN, "%S sent bad %s, it seems", conn->priv, msg->command);
}

static void m_error(conn, msg) u_conn *conn; u_msg *msg;
{
	u_log(LG_ERROR, "%S is closing connection via ERROR (%s)", conn->priv,
	      msg->argc > 0 ? msg->argv[0] : "no message");
	u_conn_error(conn, "Peer sent ERROR!");
}

static void m_svinfo(conn, msg) u_conn *conn; u_msg *msg;
{
	int tsdelta;

	if (atoi(msg->argv[0]) < 6) {
		u_conn_error(conn, "Max TS version is not 6!");
		return;
	}

	tsdelta = atoi(msg->argv[3]) - (int)NOW.tv_sec;
	if (tsdelta < 0)
		tsdelta = -tsdelta;

	if (tsdelta > 10)
		u_log(LG_WARN, "%S has TS delta of %d", conn->priv, tsdelta);
	if (tsdelta > 60) {
		u_log(LG_ERROR, "%S has excessive TS delta, killing", conn->priv);
		u_conn_error(conn, "Excessive TS delta");
		return;
	}
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

static void m_euid(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user *u;

	if (!(u = uid_generic(conn, msg)))
		return idfk(conn, msg);

	u_strlcpy(u->realhost, msg->argv[8], MAXHOST+1);
	if (msg->argv[9][0] != '*')
		u_strlcpy(u->acct, msg->argv[9], MAXACCOUNT+1);
}

static void m_uid(conn, msg) u_conn *conn; u_msg *msg;
{
	if (!uid_generic(conn, msg))
		return idfk(conn, msg);
}

u_cmd c_server[] = {
	{ "ERROR",       CTX_SERVER, m_error,         0 },
	{ "SVINFO",      CTX_SBURST, m_svinfo,        4 },

	{ "EUID",        CTX_SERVER, m_euid,         11 },
	{ "EUID",        CTX_SBURST, m_euid,         11 },
	{ "UID",         CTX_SERVER, m_uid,           9 },
	{ "UID",         CTX_SBURST, m_uid,           9 },

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
	{ "JOIN",        CTX_SERVER, not_implemented, 0 },
	{ "JUPE",        CTX_SERVER, not_implemented, 0 },
	{ "KICK",        CTX_SERVER, not_implemented, 0 },
	{ "KILL",        CTX_SERVER, not_implemented, 0 },
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
	{ "PART",        CTX_SERVER, not_implemented, 0 },
	{ "PRIVS",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "QUIT",        CTX_SERVER, not_implemented, 0 },
	{ "RESV",        CTX_SERVER, not_implemented, 0 },
	{ "SAVE",        CTX_SERVER, not_implemented, 0 },
	{ "SERVER",      CTX_SERVER, not_implemented, 0 },
	{ "SID",         CTX_SERVER, not_implemented, 0 },
	{ "SIGNON",      CTX_SERVER, not_implemented, 0 },
	{ "SJOIN",       CTX_SERVER, not_implemented, 0 },
	{ "SJOIN",       CTX_SBURST, not_implemented, 0 },
	{ "SQUIT",       CTX_SERVER, not_implemented, 0 },
	{ "STATS",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "TB",          CTX_SBURST, not_implemented, 0 },
	{ "TB",          CTX_SERVER, not_implemented, 0 },
	{ "TIME",        CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "TMODE",       CTX_SERVER, not_implemented, 0 },
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

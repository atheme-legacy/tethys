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

static void m_error(conn, msg) u_conn *conn; u_msg *msg;
{
	u_log(LG_ERROR, "%S is closing connection via ERROR (%s)", conn->priv,
	      msg->argc > 0 ? msg->argv[0] : "no message");
	u_conn_error(conn, "Peer sent ERROR!");
}

u_cmd c_server[] = {
	{ "ADMIN",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "AWAY",        CTX_SERVER, not_implemented, 0 },
	{ "BAN",         CTX_SERVER, not_implemented, 0 },
	{ "BMASK",       CTX_SERVER, not_implemented, 0 },
	{ "CHGHOST",     CTX_SERVER, not_implemented, 0 },
	{ "CONNECT",     CTX_SERVER, not_implemented, 0 },
	{ "ENCAP",       CTX_SERVER, not_implemented, 0 },
	{ "ERROR",       CTX_SERVER, m_error,         0 },
	{ "EUID",        CTX_SERVER, not_implemented, 0 },
	{ "EUID",        CTX_SBURST, not_implemented, 0 },
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
	{ "NOTICE",      CTX_SERVER, not_implemented, 0 },
	{ "OPERWALL",    CTX_SERVER, not_implemented, 0 },
	{ "PART",        CTX_SERVER, not_implemented, 0 },
	{ "PING",        CTX_SERVER, not_implemented, 0 }, /* hunted? */
	{ "PING",        CTX_SBURST, not_implemented, 0 },
	{ "PONG",        CTX_SERVER, not_implemented, 0 }, /* hunted? */
	{ "PONG",        CTX_SBURST, not_implemented, 0 },
	{ "PRIVMSG",     CTX_SERVER, not_implemented, 0 },
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
	{ "SVINFO",      CTX_SBURST, not_implemented, 0 },
	{ "TB",          CTX_SBURST, not_implemented, 0 },
	{ "TB",          CTX_SERVER, not_implemented, 0 },
	{ "TIME",        CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "TMODE",       CTX_SERVER, not_implemented, 0 },
	{ "TOPIC",       CTX_SERVER, not_implemented, 0 },
	{ "TRACE",       CTX_SERVER, not_implemented, 0 }, /* hunted */
	{ "UID",         CTX_SERVER, not_implemented, 0 },
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

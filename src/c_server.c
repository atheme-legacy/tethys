/* Tethys, c_server.c -- server commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int not_implemented(u_conn *conn, u_msg *msg)
{
	u_log(LG_SEVERE, "%S used unimplemented S2S command %s",
	      conn->priv, msg->command);
	return 0;
}

static int idfk(u_conn *conn, u_msg *msg)
{
	u_log(LG_WARN, "%S sent bad %s, it seems", conn->priv, msg->command);
	return 0;
}

static int m_error(u_conn *conn, u_msg *msg)
{
	u_log(LG_ERROR, "%S is closing connection via ERROR (%s)", conn->priv,
	      msg->argc > 0 ? msg->argv[0] : "no message");
	u_conn_fatal(conn, "Peer sent ERROR!");
	return 0;
}

u_cmd c_server[] = {
	{ "ERROR",       CTX_SERVER, m_error,         0, 0 },
	{ "SVINFO",      CTX_SERVER, m_svinfo,        4, 0 },

	{ "EUID",        CTX_SERVER, m_euid,         11, 0, CMD_PROP_BROADCAST  },

	{ "SJOIN",       CTX_SERVER, m_sjoin,         4, 0 },
	{ "JOIN",        CTX_SERVER, m_join,          3, 0 },
	{ "PART",        CTX_SERVER, m_part,          1, 0 },

	{ "TMODE",       CTX_SERVER, m_tmode,         3, 0 },

	{ "KILL",        CTX_SERVER, m_kill,          1, 0 },
	{ "QUIT",        CTX_SERVER, m_quit,          0, 0 },

	{ "SID",         CTX_SERVER, m_sid,           4, 0, CMD_PROP_BROADCAST  },
/*	disabled temporarily, as this conflicts with SERVER in c_reg
 	{ "SERVER",      CTX_SERVER, m_server,        3, 0, CMD_PROP_BROADCAST  },*/

	{ "INVITE",      CTX_SERVER, m_invite,        2, 0 },

	{ "ADMIN",       CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "BAN",         CTX_SERVER, not_implemented, 0, 0 },
	{ "BMASK",       CTX_SERVER, not_implemented, 0, 0 },
	{ "CHGHOST",     CTX_SERVER, not_implemented, 0, 0 },
	{ "CONNECT",     CTX_SERVER, not_implemented, 0, 0 },
	{ "ENCAP",       CTX_SERVER, not_implemented, 0, 0 },
	{ "GLINE",       CTX_SERVER, not_implemented, 0, 0 },
	{ "GUNGLINE",    CTX_SERVER, not_implemented, 0, 0 },
	{ "INFO",        CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "JUPE",        CTX_SERVER, not_implemented, 0, 0 },
	{ "KICK",        CTX_SERVER, not_implemented, 0, 0 },
	{ "KLINE",       CTX_SERVER, not_implemented, 0, 0 },
	{ "KNOCK",       CTX_SERVER, not_implemented, 0, 0 },
	{ "LINKS",       CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "LOCOPS",      CTX_SERVER, not_implemented, 0, 0 },
	{ "LUSERS",      CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "MLOCK",       CTX_SERVER, not_implemented, 0, 0 },
	{ "MODE",        CTX_SERVER, not_implemented, 0, 0 },
	{ "MOTD",        CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "NICK",        CTX_SERVER, not_implemented, 0, 0 },
	{ "NICKDELAY",   CTX_SERVER, not_implemented, 0, 0 },
	{ "OPERWALL",    CTX_SERVER, not_implemented, 0, 0 },
	{ "PRIVS",       CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "RESV",        CTX_SERVER, not_implemented, 0, 0 },
	{ "SAVE",        CTX_SERVER, not_implemented, 0, 0 },
	{ "SIGNON",      CTX_SERVER, not_implemented, 0, 0 },
	{ "SQUIT",       CTX_SERVER, not_implemented, 0, 0 },
	{ "STATS",       CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "TB",          CTX_SERVER, not_implemented, 0, 0 },
	{ "TIME",        CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "TOPIC",       CTX_SERVER, not_implemented, 0, 0 },
	{ "TRACE",       CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "UNKLINE",     CTX_SERVER, not_implemented, 0, 0 },
	{ "UNRESV",      CTX_SERVER, not_implemented, 0, 0 },
	{ "UNXLINE",     CTX_SERVER, not_implemented, 0, 0 },
	{ "USERS",       CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "VERSION",     CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "WALLOPS",     CTX_SERVER, not_implemented, 0, 0 },
	{ "WHOIS",       CTX_SERVER, not_implemented, 0, 0 }, /* hunted */
	{ "XLINE",       CTX_SERVER, not_implemented, 0, 0 },
	{ "" }
};

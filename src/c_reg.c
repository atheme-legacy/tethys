/* ircd-micro, c_reg.c -- connection registration commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int err_already(conn, msg) u_conn *conn; u_msg *msg;
{
	return u_conn_num(conn, ERR_ALREADYREGISTERED);
}

static int gtfo(conn, msg) u_conn *conn; u_msg *msg;
{
	u_conn_error(conn, "Server doesn't know what it's doing");
	return 0;
}

static int m_pass(conn, msg) u_conn *conn; u_msg *msg;
{
	if (msg->argc != 1 && msg->argc != 4) {
		u_conn_num(conn, ERR_NEEDMOREPARAMS, msg->command);
		return 0;
	}

	if (conn->pass != NULL)
		free(conn->pass);
	conn->pass = u_strdup(msg->argv[0]);

	if (msg->argc == 1)
		return 0;

	if (!streq(msg->argv[1], "TS") || !streq(msg->argv[2], "6")) {
		u_conn_error(conn, "Invalid TS version");
		return 0;
	}

	if (!is_valid_sid(msg->argv[3])) {
		u_conn_error(conn, "Invalid SID");
		return 0;
	}

	u_server_make_sreg(conn, msg->argv[3]);
	return 0;
}

static int try_reg(conn) u_conn *conn;
{
	u_user *u = conn->priv;

	if (u_user_state(u, 0) != USER_REGISTERING || !u->nick[0]
			|| !u->ident[0] || !u->gecos[0])
		return 0;

	conn->auth = u_find_auth(conn);
	if (conn->auth == NULL) {
		if (conn->pass) {
			u_conn_num(conn, ERR_PASSWDMISMATCH);
		} else {
			u_conn_f(conn, ":%S NOTICE %U :*** %s",
			         &me, u, "No auth blocks for your host");
		}
		return 0;
	}

	u_user_welcome(u);
	return 0;
}

static int m_nick(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user_local *ul;
	char buf[MAXNICKLEN+1];

	u_user_make_ureg(conn);
	ul = conn->priv;

	u_strlcpy(buf, msg->argv[0], MAXNICKLEN+1);

	if (!is_valid_nick(buf)) {
		u_conn_num(conn, ERR_ERRONEOUSNICKNAME, buf);
		return 0;
	}

	if (u_user_by_nick(buf)) {
		u_conn_num(conn, ERR_NICKNAMEINUSE, buf);
		return 0;
	}

	u_user_set_nick(USER(ul), buf, NOW.tv_sec);

	try_reg(conn);
	return 0;
}

static int m_user(conn, msg) u_conn *conn; u_msg *msg;
{
	u_user_local *ul;
	char buf[MAXIDENT+1];

	u_user_make_ureg(conn);
	ul = conn->priv;

	u_strlcpy(buf, msg->argv[0], MAXIDENT+1);
	if (!is_valid_ident(buf)) {
		u_conn_f(conn, "invalid ident");
		return 0;
	}
	strcpy(USER(ul)->ident, buf);
	u_strlcpy(USER(ul)->gecos, msg->argv[3], MAXGECOS+1);

	try_reg(conn);
	return 0;
}

struct cap {
	char name[32];
	ulong flag;
} caps[] = {
	{ "multi-prefix",   CAP_MULTI_PREFIX },
	{ "away-notify",    CAP_AWAY_NOTIFY },
	{ "" }
};

static int cap_add(u, cap) u_user *u; char *cap;
{
	struct cap *cur;
	char *s;

	for (s=cap; *s; s++)
		*s = isupper(*s) ? tolower(*s) : *s;

	for (cur=caps; cur->name[0]; cur++) {
		if (!streq(cur->name, cap))
			continue;
		u->flags |= CAP_MULTI_PREFIX;
		return 0;
	}

	return 1;
}

static int m_cap(conn, msg) u_conn *conn; u_msg *msg;
{
	char ackbuf[BUFSIZE];
	char nakbuf[BUFSIZE];
	u_user_local *ul;
	char *s, *p, *q;
	struct cap *cur;

	u_user_make_ureg(conn);
	ul = conn->priv;
	u_user_state(USER(ul), USER_CAP_NEGOTIATION);

	ascii_canonize(msg->argv[0]);

	if (streq(msg->argv[0], "LS")) {
		/* maybe this will need to be wrapped? */
		ackbuf[0] = '\0';
		for (cur=caps; cur->name[0]; cur++) {
			u_strlcat(ackbuf, " ", BUFSIZE);
			u_strlcat(ackbuf, cur->name, BUFSIZE);
		}
		u_conn_f(conn, ":%S CAP * LS :%s", &me, ackbuf+1);

	} else if (streq(msg->argv[0], "REQ")) {
		if (msg->argc != 2) {
			u_conn_num(conn, ERR_NEEDMOREPARAMS, "CAP");
			return 0;
		}

		p = msg->argv[1];
		ackbuf[0] = ackbuf[1] = '\0';
		nakbuf[0] = nakbuf[1] = '\0';
		while ((s = cut(&p, " \t")) != NULL) {
			q = ackbuf;
			if (!cap_add(USER(ul), s))
				q = nakbuf;
			u_strlcat(q, " ", BUFSIZE);
			u_strlcat(q, s, BUFSIZE);
		}

		if (ackbuf[1])
			u_conn_f(conn, ":%S CAP * ACK :%s", &me, ackbuf+1);
		if (nakbuf[1])
			u_conn_f(conn, ":%S CAP * NAK :%s", &me, nakbuf+1);

	} else if (streq(msg->argv[0], "END")) {
		u_user_state(USER(ul), USER_REGISTERING);

	}

	try_reg(conn);
	return 0;
}

static int try_serv(conn) u_conn *conn;
{
	u_server *sv = conn->priv;
	u_link *link;
	uint capab_need = CAPAB_QS | CAPAB_EX | CAPAB_IE | CAPAB_EUID | CAPAB_ENCAP;

	if ((sv->capab & capab_need) != capab_need) {
		u_conn_error(conn, "Don't have all needed CAPABs!");
		return 0;
	}

	if (!(link = u_find_link(conn))) {
		u_conn_error(conn, "No link{} blocks for your host");
		return 0;
	}

	u_roster_f(R_SERVERS, conn, ":%S SID %s %d %s :%s", &me,
	           sv->name, sv->hops, sv->sid, sv->desc);
	u_server_burst(sv, link);
	return 0;
}

static int m_capab(conn, msg) u_conn *conn; u_msg *msg;
{
	u_server *sv = conn->priv;
	u_server_add_capabs(sv, msg->argv[0]);
	return 0;
}

static int m_server(conn, msg) u_conn *conn; u_msg *msg;
{
	u_server *sv = conn->priv;

	u_strlcpy(sv->name, msg->argv[0], MAXSERVNAME+1);
	u_strlcpy(sv->desc, msg->argv[2], MAXSERVDESC+1);

	try_serv(conn);

	return 0;
}

u_cmd c_reg[] = {
	{ "PASS",    CTX_UNREG,  m_pass, 1 },

	{ "PASS",    CTX_UREG,   m_pass, 1 },
	{ "PASS",    CTX_USER,   err_already, 0 },
	{ "NICK",    CTX_UNREG,  m_nick, 1 },
	{ "NICK",    CTX_UREG,   m_nick, 1 },
	{ "USER",    CTX_UNREG,  m_user, 4 },
	{ "USER",    CTX_UREG,   m_user, 4 },
	{ "USER",    CTX_USER,   err_already, 0 },
	{ "CAP",     CTX_UNREG,  m_cap,  1 },
	{ "CAP",     CTX_UREG,   m_cap,  1 },
	{ "CAP",     CTX_USER,   err_already, 0 },

	{ "CAPAB",   CTX_SREG,   m_capab, 1 },
	{ "CAPAB",   CTX_UNREG,  gtfo, 0 },
	{ "SERVER",  CTX_SREG,   m_server, 3 },
	{ "SERVER",  CTX_UNREG,  gtfo, 0 },

	{ "" }
};

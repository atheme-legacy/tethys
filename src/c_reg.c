/* Tethys, c_reg.c -- connection registration commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int err_already(u_conn *conn, u_msg *msg)
{
	return u_conn_num(conn, ERR_ALREADYREGISTERED);
}

static int gtfo(u_conn *conn, u_msg *msg)
{
	u_conn_fatal(conn, "Server doesn't know what it's doing");
	return 0;
}

static int m_pass(u_conn *conn, u_msg *msg)
{
	if (conn->flags & U_CONN_REGISTERED)
		return err_already(conn, msg);

	if (msg->argc != 1 && msg->argc != 4) {
		u_conn_num(conn, ERR_NEEDMOREPARAMS, msg->command);
		return 0;
	}

	if (conn->pass != NULL)
		free(conn->pass);
	conn->pass = strdup(msg->argv[0]);

	if (msg->argc == 1)
		return 0;

	if (!streq(msg->argv[1], "TS") || !streq(msg->argv[2], "6")) {
		u_conn_fatal(conn, "Invalid TS version");
		return 0;
	}

	if (!is_valid_sid(msg->argv[3])) {
		u_conn_fatal(conn, "Invalid SID");
		return 0;
	}

	u_server_make_sreg(conn, msg->argv[3]);
	return 0;
}

static int try_reg(u_conn *conn)
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

	conn->flags |= U_CONN_REGISTERED;

	u_user_welcome(USER_LOCAL(u));
	return 0;
}

static int m_nick(u_conn *conn, u_msg *msg)
{
	u_user_local *ul;
	char buf[MAXNICKLEN+1];

	if (conn->flags & U_CONN_REGISTERED)
		return err_already(conn, msg);

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

static int m_user(u_conn *conn, u_msg *msg)
{
	u_user_local *ul;
	char buf[MAXIDENT+1];

	if (conn->flags & U_CONN_REGISTERED)
		return err_already(conn, msg);

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

static char caps_str[512];

static int cap_add(u_user *u, char *cap)
{
	struct cap *cur;
	char *s;

	for (s=cap; *s; s++)
		*s = isupper(*s) ? tolower(*s) : *s;

	for (cur=caps; cur->name[0]; cur++) {
		if (!streq(cur->name, cap))
			continue;
		u->flags |= cur->flag;
		return 1;
	}

	return 0;
}

static void suspend_registration(u_user_local *ul)
{
	if (u_user_state(USER(ul), 0) == USER_REGISTERING)
		u_user_state(USER(ul), USER_CAP_NEGOTIATION);
}

static void resume_registration(u_user_local *ul)
{
	if (u_user_state(USER(ul), 0) == USER_CAP_NEGOTIATION)
		u_user_state(USER(ul), USER_REGISTERING);
}

static int m_cap(u_conn *conn, u_msg *msg)
{
	u_user_local *ul;
	u_user *u;
	char *s, *p, buf[512];
	struct cap *cur;

	u_user_make_ureg(conn);
	ul = conn->priv;
	u = USER(ul);

	ascii_canonize(msg->argv[0]);

	if (streq(msg->argv[0], "LS")) {
		/* maybe this will need to be wrapped? */
		if (!caps_str[0]) {
			caps_str[0] = '\0';
			for (cur=caps; cur->name[0]; cur++) {
				if (caps_str[0])
					u_strlcat(caps_str, " ", BUFSIZE);
				u_strlcat(caps_str, cur->name, BUFSIZE);
			}
			u_log(LG_FINE, "Built CAPs list: %s", caps_str);
		}

		suspend_registration(ul);
		u_conn_f(conn, ":%S CAP %U LS :%s", &me, u, caps_str);

	} else if (streq(msg->argv[0], "REQ")) {
		int ack = 1;
		uint old = u->flags;

		if (msg->argc != 2) {
			u_conn_num(conn, ERR_NEEDMOREPARAMS, "CAP");
			return 0;
		}

		u_strlcpy(buf, msg->argv[1], BUFSIZE);
		p = buf;
		while ((s = cut(&p, " \t")) != NULL) {
			if (!cap_add(u, s)) {
				u->flags = old;
				ack = 0;
				break;
			}
		}

		u_log(LG_FINE, "%U flags: %x", u, u->flags);

		suspend_registration(ul);
		u_conn_f(conn, ":%S CAP %U %s :%s", &me, u,
		         ack ? "ACK" : "NAK", msg->argv[1]);

	} else if (streq(msg->argv[0], "END")) {
		resume_registration(ul);

	} else if (streq(msg->argv[0], "LIST")) {
		p = buf;

		u_strlcpy(buf, " ", 512);
		for (cur=caps; cur->name[0]; cur++) {
			if (u->flags & cur->flag)
				p += sprintf(p, " %s", cur->name);
		}

		u_conn_f(conn, ":%S CAP %U LIST :%s", &me, u, buf + 1);
	}

	/* this is harmless if called on anything other than connections
	   in USER_REGISTERING */
	try_reg(conn);

	return 0;
}

static int try_serv(u_conn *conn)
{
	u_server *sv = conn->priv;
	u_link *link;
	uint capab_need = CAPAB_QS | CAPAB_EX | CAPAB_IE | CAPAB_EUID | CAPAB_ENCAP;

	if ((sv->capab & capab_need) != capab_need) {
		u_conn_fatal(conn, "Don't have all needed CAPABs!");
		return 0;
	}

	if (!(link = u_find_link(conn))) {
		u_conn_fatal(conn, "No link{} blocks for your host");
		return 0;
	}

	conn->flags |= U_CONN_REGISTERED;

	u_roster_f(R_SERVERS, conn, ":%S SID %s %d %s :%s", &me,
	           sv->name, sv->hops, sv->sid, sv->desc);
	u_server_burst_1(sv, link);
	u_server_burst_2(sv, link);
	return 0;
}

static int m_capab(u_conn *conn, u_msg *msg)
{
	if (conn->flags & U_CONN_REGISTERED) {
		u_conn_fatal(conn, "Server sent CAPAB after registration");
		return 0;
	}

	u_server *sv = conn->priv;
	u_server_add_capabs(sv, msg->argv[0]);
	return 0;
}

static int m_server(u_conn *conn, u_msg *msg)
{
	u_server *sv = conn->priv;

	u_strlcpy(sv->name, msg->argv[0], MAXSERVNAME+1);
	u_strlcpy(sv->desc, msg->argv[2], MAXSERVDESC+1);

	try_serv(conn);

	return 0;
}

u_cmd c_reg[] = {
	{ "PASS",    CTX_UNREG,     m_pass,   1, CMD_UNREGISTERED, },

	{ "PASS",    CTX_USER,      m_pass,   0, CMD_UNREGISTERED, },
	{ "NICK",    CTX_UNREG,     m_nick,   1, CMD_UNREGISTERED, },
	{ "NICK",    CTX_USER,      m_nick,   1, CMD_UNREGISTERED, },
	{ "USER",    CTX_UNREG,     m_user,   4, CMD_UNREGISTERED, },
	{ "USER",    CTX_USER,      m_user,   4, CMD_UNREGISTERED, },
	{ "CAP",     CTX_UNREG,     m_cap,    1, CMD_UNREGISTERED, },
	{ "CAP",     CTX_USER,      m_cap,    1, CMD_UNREGISTERED, },

	{ "CAPAB",   CTX_SERVER,    m_capab,  1, CMD_UNREGISTERED, },
	{ "CAPAB",   CTX_UNREG,     gtfo,     0, CMD_UNREGISTERED, },
	{ "SERVER",  CTX_SERVER,    m_server, 3, CMD_UNREGISTERED, },
	{ "SERVER",  CTX_UNREG,     gtfo,     0, CMD_UNREGISTERED, },

	{ "" }
};

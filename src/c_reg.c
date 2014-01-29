/* Tethys, c_reg.c -- connection registration commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

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

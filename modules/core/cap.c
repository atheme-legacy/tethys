/* Tethys, core/cap -- CAP command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING  file in the project root */

#include "ircd.h"

/* TODO: move this to *core*! we're an ircv3 ircd! */

struct cap {
	char name[32];
	ulong flag;
} caps[] = {
	{ "multi-prefix",   CAP_MULTI_PREFIX },
	{ "away-notify",    CAP_AWAY_NOTIFY },
	{ }
};

static char caps_str[512];

static int cap_add(u_user *u, char *cap)
{
	struct cap *cur;
	char *s;

	for (s=cap; *s; s++)
		*s = tolower(*s);

	for (cur=caps; cur->name[0]; cur++) {
		if (!streq(cur->name, cap))
			continue;

		u->flags |= cur->flag;
		return 1;
	}

	return 0;
}

static int c_lu_cap(u_sourceinfo *si, u_msg *msg)
{
	char *s, *p, buf[512];
	struct cap *cur;
	char *subcmd = msg->argv[0];
	u_strop_state st;

	ascii_canonize(subcmd);

	if (streq(subcmd, "LS")) {
		/* will need to be wrapped, possibly */
		if (!caps_str[0]) {
			caps_str[0] = '\0';
			for (cur = caps; cur->name[0]; cur++) {
				if (caps_str[0])
					u_strlcat(caps_str, " ", BUFSIZE);
				u_strlcat(caps_str, cur->name, BUFSIZE);
			}
			u_log(LG_FINE, "Built CAPs list: %s", caps_str);
		}

		si->u->flags |= USER_WAIT_CAPS;
		u_conn_f(si->link, ":%S CAP %U LS :%s", &me, si->u, caps_str);

	} else if (streq(subcmd, "REQ")) {
		int ack = 1;
		uint old = si->u->flags;

		if (msg->argc != 2) {
			u_src_num(si, ERR_NEEDMOREPARAMS, "CAP");
			return 0;
		}

		u_strlcpy(buf, msg->argv[1], BUFSIZE);
		U_STROP_SPLIT(&st, buf, " \t", &s) {
			if (!cap_add(si->u, s)) {
				si->u->flags = old;
				ack = 0;
				break;
			}
		}

		u_log(LG_FINE, "%U flags: %x", si->u, si->u->flags);

		si->u->flags |= USER_WAIT_CAPS;
		u_conn_f(si->link, ":%S CAP %U %s :%s", &me, si->u,
		         ack ? "ACK" : "NAK", msg->argv[1]);

	} else if (streq(msg->argv[0], "END")) {
		si->u->flags &= ~USER_WAIT_CAPS;

	} else if (streq(msg->argv[0], "LIST")) {
		p = buf;

		u_strlcpy(buf, " ", 512);
		for (cur=caps; cur->name[0]; cur++) {
			if (si->u->flags & cur->flag)
				p += sprintf(p, " %s", cur->name);
		}

		u_conn_f(si->link, ":%S CAP %U LIST :%s", &me, si->u, buf + 1);
	}

	/* harmless if already registered */
	u_user_try_register(si->u);

	return 0;
}

static u_cmd cap_cmdtab[] = {
	{ "CAP", SRC_FIRST, u_repeat_as_user, 0 },
	{ "CAP", SRC_LOCAL_USER | SRC_UNREGISTERED_USER, c_lu_cap, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/cap", "Alex Iadicicco", "CAP command",
	NULL, NULL, cap_cmdtab);

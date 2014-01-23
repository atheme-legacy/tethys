/* Tethys, core/nick.c -- NICK command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_uu_nick(u_sourceinfo *si, u_msg *msg)
{
	char buf[MAXNICKLEN+1];

	u_strlcpy(buf, msg->argv[0], MAXNICKLEN+1); /* truncate */

	if (!is_valid_nick(buf)) {
		u_conn_num(si->source, ERR_ERRONEOUSNICKNAME, buf);
		return 0;
	}

	if (u_user_by_nick(buf)) {
		u_conn_num(si->source, ERR_NICKNAMEINUSE, buf);
		return 0;
	}

	u_user_set_nick(si->u, buf, NOW.tv_sec);

	u_user_try_register(si->u);

	return 0;
}

static int c_lu_nick(u_sourceinfo *si, u_msg *msg)
{
	char *s, *newnick = msg->argv[0];

	/* cut newnick to nicklen */
	if (strlen(newnick) > MAXNICKLEN)
		newnick[MAXNICKLEN] = '\0';

	if (!is_valid_nick(newnick))
		return u_user_num(si->u, ERR_ERRONEOUSNICKNAME, newnick);

	/* due to the scandalous origins, (~ being uppercase of ^) and ~
	 * being disallowed as a nick char, we need to chop the first ~
	 * instead of just erroring.
	 */
	if ((s = strchr(newnick, '~')))
		*s = '\0';

	/* check for nick in use and *not* because of a case change */
	if (irccmp(si->u->nick, newnick) && u_user_by_nick(newnick))
		return u_user_num(si->u, ERR_NICKNAMEINUSE, newnick);

	if (streq(si->u->nick, newnick))
		return 0;

	/* Send these BEFORE clobbered --Elizabeth */
	u_sendto_visible(si->u, ST_USERS, ":%H NICK :%s", si->u, newnick);
	u_roster_f(R_SERVERS, NULL, ":%H NICK %s %u", si->u, newnick, NOW.tv_sec);
	u_conn_f(si->source, ":%H NICK :%s", si->u, newnick);

	u_user_set_nick(si->u, newnick, NOW.tv_sec);

	return 0;
}

static int c_ru_nick(u_sourceinfo *si, u_msg *msg)
{
	char *newnick = msg->argv[0];

	/* TODO: resolve collisions */

	u_sendto_visible(si->u, ST_USERS, ":%H NICK :%s", si->u, newnick);
	msg->propagate = CMD_DO_BROADCAST;

	u_user_set_nick(si->u, newnick, atoi(msg->argv[1]));

	return 0;
}

static u_cmd nick_cmdtab[] = {
	{ "NICK", SRC_FIRST,              u_repeat_as_user, 0 },
	{ "NICK", SRC_UNREGISTERED_USER,  c_uu_nick, 1 },
	{ "NICK", SRC_LOCAL_USER,         c_lu_nick, 1 },
	{ "NICK", SRC_REMOTE_USER,        c_ru_nick, 2, CMD_PROP_BROADCAST },
	{ }
};

static int init_nick(u_module *m)
{
	u_cmds_reg(nick_cmdtab);
}

TETHYS_MODULE_V1(
	"core/nick", "Alex Iadicicco", "NICK command",
	init_nick, NULL);

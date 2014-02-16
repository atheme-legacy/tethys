/* Tethys, message.c -- PRIVMSG and NOTICE
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int message_blocked(u_chan *c, u_user *u)
{
	u_chanuser *cu = u_chan_user_find(c, u);

	if (!cu) {
		if (c->mode & CMODE_NOEXTERNAL) {
			u_user_num(u, ERR_CANNOTSENDTOCHAN, c, 'n');
			return 1;
		}
	} else if (u_is_muted(cu)) {
		/* TODO: +z */
		u_user_num(u, ERR_CANNOTSENDTOCHAN, c, 'm');
		return 1;
	}

	return 0;
}

static int message_chan(u_sourceinfo *si, u_msg *msg)
{
	u_chan *tgt;

	if (!(tgt = u_chan_get(msg->argv[0])))
		return u_user_num(si->u, ERR_NOSUCHCHANNEL, msg->argv[0]);

	if (si->source->type == LINK_USER && message_blocked(tgt, si->u))
		return 0;

	if (SRC_IS_USER(si)) {
		u_sendto_chan(tgt, si->source, ST_ALL, ":%H %s %C :%s",
		              si->u, msg->command, tgt, msg->argv[1]);
	} else {
		u_sendto_chan(tgt, si->source, ST_ALL, ":%S NOTICE %C :%s",
		              si->s, tgt, msg->argv[1]);
	}

	return 0;
}

static int message_user(u_sourceinfo *si, u_msg *msg)
{
	u_user *tu;

	if (!(tu = u_user_by_ref(si->source, msg->argv[0])))
		return u_user_num(si->u, ERR_NOSUCHNICK, msg->argv[0]);

	if (tu->link == si->source) {
		u_log(LG_ERROR, "%s came from destination??", msg->command);
		return 0;
	}

	if (SRC_IS_USER(si)) {
		u_link_f(tu->link, ":%H %s %U :%s", si->u, msg->command,
		         tu, msg->argv[1]);
	} else {
		u_link_f(tu->link, ":%S NOTICE %U :%s", si->s,
		         tu, msg->argv[1]);
	}

	return 0;
}

static int c_a_message(u_sourceinfo *si, u_msg *msg)
{
	if (strchr(CHANTYPES, msg->argv[0][0]))
		return message_chan(si, msg);

	return message_user(si, msg);
}

static u_cmd message_cmdtab[] = {
	/* I'm pretty sure this will drop a PRIVMSG from a server */
	{ "PRIVMSG",  SRC_USER,  c_a_message, 2, 0, U_RATELIMIT_STD },
	{ "NOTICE",   SRC_ANY,   c_a_message, 2, 0, U_RATELIMIT_STD },
	{ },
};

TETHYS_MODULE_V1(
	"core/message", "Alex Iadicicco", "PRIVMSG and NOTICE",
	NULL, NULL, message_cmdtab);

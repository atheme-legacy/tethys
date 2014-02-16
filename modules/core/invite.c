/* Tethys, core/invite -- INVITE command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_u_invite(u_sourceinfo *si, u_msg *msg)
{
	u_user *tu;
	u_chan *c;
	u_chanuser *cu;

	if (!(tu = u_user_by_ref(si->source, msg->argv[0])))
		return u_src_num(si, ERR_NOSUCHNICK, msg->argv[0]);
	if (!(c = u_chan_get(msg->argv[1])))
		return u_src_num(si, ERR_NOSUCHCHANNEL, msg->argv[1]);

	if (SRC_IS_LOCAL_USER(si)) {
		if (!(cu = u_chan_user_find(c, si->u)))
			return u_src_num(si, ERR_NOTONCHANNEL, c);
		if (u_chan_user_find(c, tu))
			return u_src_num(si, ERR_USERONCHANNEL, tu, c);
		if (!(cu->flags & CU_PFX_OP) && !(c->mode & CMODE_FREEINVITE))
			return u_src_num(si, ERR_CHANOPRIVSNEEDED);
	} else if (msg->argc > 2) {
		int ts = atoi(msg->argv[2]);
		if (ts > c->ts) {
			u_log(LG_INFO, "Dropping s2s INVITE for newer channel");
			return 0;
		}
	} else {
		u_log(LG_WARN, "%G sent s2s INVITE without channel TS",
		      si->source);
	}

	if (IS_LOCAL_USER(tu)) {
		u_add_invite(c, tu);
		u_link_f(tu->link, ":%I INVITE %U :%C", si, tu, c);
	} else {
		u_link_f(tu->link, ":%I INVITE %U %C :%u", si, tu, c, c->ts);
	}

	if (SRC_IS_LOCAL_USER(si))
		u_src_num(si, RPL_INVITING, tu, c);

	return 0;
}

static u_cmd invite_cmdtab[] = {
	{ "INVITE", SRC_USER, c_u_invite, 2 },
	{ }
};

TETHYS_MODULE_V1(
	"core/invite", "Alex Iadicicco", "INVITE command",
	NULL, NULL, invite_cmdtab);

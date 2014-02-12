/* Tethys, core/kick -- KICK command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_a_kick(u_sourceinfo *si, u_msg *msg)
{
	u_user *tu;
	u_chan *c;
	u_chanuser *tcu, *cu;
	char *r = msg->argv[2];

	if (!(c = u_chan_get(msg->argv[0])))
		return u_src_num(si, ERR_NOSUCHCHANNEL, msg->argv[0]);
	if (!(tu = u_user_by_ref(si->source, msg->argv[1])))
		return u_src_num(si, ERR_NOSUCHNICK, msg->argv[1]);
	if (SRC_IS_LOCAL_USER(si)) {
		if (!(cu = u_chan_user_find(c, si->u)))
			return u_src_num(si, ERR_NOTONCHANNEL, c);
		if (!(cu->flags & CU_PFX_OP))
			return u_src_num(si, ERR_CHANOPRIVSNEEDED, c);
	}
	if (!(tcu = u_chan_user_find(c, tu)))
		return u_src_num(si, ERR_USERNOTINCHANNEL, tu, c);

	r = r ? r : tu->nick;
	u_sendto_chan(c, NULL, ST_USERS, ":%I KICK %C %U :%s", si, c, tu, r);
	u_sendto_servers(si->source, ":%I KICK %C %U :%s", si, c, tu, r);

	u_chan_user_del(tcu);

	return 0;
}

static u_cmd kick_cmdtab[] = {
	{ "KICK", SRC_ANY, c_a_kick, 2 },
	{ }
};

TETHYS_MODULE_V1(
	"core/kick", "Alex Iadicicco", "KICK command",
	NULL, NULL, kick_cmdtab);

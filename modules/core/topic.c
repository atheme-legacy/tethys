/* Tethys, core/topic -- TOPIC command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* This function could, at the time of writing this comment, sanely handle
   SRC_ANY, but ts6-protocol.txt says that TOPIC can only come from users */
static int c_u_topic(u_sourceinfo *si, u_msg *msg)
{
	u_chan *c;
	u_chanuser *cu;

	if (!(c = u_chan_get(msg->argv[0])))
		return u_src_num(si, ERR_NOSUCHCHANNEL, msg->argv[0]);

	cu = u_chan_user_find(c, si->u);

	if (msg->argc == 1) {
		if (!cu && (c->flags & CMODE_SECRET))
			return u_src_num(si, ERR_NOTONCHANNEL, c);
		return u_chan_send_topic(c, si->u);
	}

	if (SRC_IS_LOCAL_USER(si)) {
		if (!cu)
			return u_src_num(si, ERR_NOTONCHANNEL, c);
		else if ((c->mode & CMODE_TOPIC) && !(cu->flags & CU_PFX_OP))
			return u_src_num(si, ERR_CHANOPRIVSNEEDED, c);
	}

	u_strlcpy(c->topic, msg->argv[1], MAXTOPICLEN+1);
	u_strlcpy(c->topic_setter, (char*)si->name, MAXNICKLEN+1);
	c->topic_time = NOW.tv_sec;

	u_sendto_chan(c, NULL, ST_USERS, ":%I TOPIC %C :%s", si, c, c->topic);
	u_sendto_servers(si->source, ":%I TOPIC %C :%s", si, c, c->topic);

	return 0;
}

static u_cmd topic_cmdtab[] = {
	{ "TOPIC", SRC_USER, c_u_topic, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/topic", "Alex Iadicicco", "TOPIC command",
	NULL, NULL, topic_cmdtab);

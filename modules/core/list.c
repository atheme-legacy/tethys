/* Tethys, core/list -- LIST command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int list_entry(u_sourceinfo *si, u_chan *c)
{
	if ((c->mode & (CMODE_PRIVATE | CMODE_SECRET))
	    && !u_chan_user_find(c, si->u))
		return 0;

	u_src_num(si, RPL_LIST, c->name, c->members->size, c->topic);
	return 0;
}

static int c_lu_list(u_sourceinfo *si, u_msg *msg)
{
	mowgli_patricia_iteration_state_t state;
	u_chan *c;

	if (msg->argc > 0) {
		if (!(c = u_chan_get(msg->argv[0])))
			return u_src_num(si, ERR_NOSUCHCHANNEL, msg->argv[0]);

		u_src_num(si, RPL_LISTSTART);
		list_entry(si, c);
		u_src_num(si, RPL_LISTEND);
		return 0;
	}

	u_src_num(si, RPL_LISTSTART);
	MOWGLI_PATRICIA_FOREACH(c, &state, all_chans) {
		if (c->members->size < 3)
			continue;

		list_entry(si, c);
	}
	u_src_num(si, RPL_LISTEND);

	return 0;
}

static u_cmd list_cmdtab[] = {
	{ "LIST", SRC_LOCAL_USER, c_lu_list, 0 },
	{ }
};

TETHYS_MODULE_V1(
	"core/list", "Alex Iadicicco", "LIST command",
	NULL, NULL, list_cmdtab);

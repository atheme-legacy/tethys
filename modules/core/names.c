/* Tethys, core/names -- NAMES command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_lu_names(u_sourceinfo *si, u_msg *msg)
{
	u_chan *c;

	/* TODO: no arguments version */
	if (msg->argc == 0)
		return 0;

	if (!(c = u_chan_get(msg->argv[0])))
		return u_src_num(si, ERR_NOSUCHCHANNEL, msg->argv[0]);

	u_chan_send_names(c, si->u);
	return 0;
}

static u_cmd names_cmdtab[] = {
	{ "NAMES", SRC_LOCAL_USER, c_lu_names, 0 },
	{ }
};

TETHYS_MODULE_V1(
	"core/names", "Alex Iadicicco", "NAMES command",
	NULL, NULL, names_cmdtab);

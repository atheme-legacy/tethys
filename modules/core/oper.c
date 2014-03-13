/* Tethys, core/oper -- OPER command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_lu_oper(u_sourceinfo *si, u_msg *msg)
{
	u_oper_block *oper;

	if (!(oper = u_find_oper(si->source->conf.auth,
	                         msg->argv[0], msg->argv[1])))
		return u_user_num(si->u, ERR_NOOPERHOST);

	oper_up(si, oper);
	return 0;
}

static u_cmd oper_cmdtab[] = {
	{ "OPER", SRC_LOCAL_USER, c_lu_oper, 2 },
	{ }
};

TETHYS_MODULE_V1(
	"core/oper", "Alex Iadicicco", "OPER command",
	NULL, NULL, oper_cmdtab);

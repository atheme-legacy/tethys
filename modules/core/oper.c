/* Tethys, core/oper -- OPER command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_lu_oper(u_sourceinfo *si, u_msg *msg)
{
	u_oper *oper;

	if (!(oper = u_find_oper(si->source->auth, msg->argv[0], msg->argv[1])))
		return u_user_num(si->u, ERR_NOOPERHOST);

	si->u->oper = oper;
	si->u->mode |= UMODE_OPER;
	u_conn_f(si->source, ":%U MODE %U :+o", si->u, si->u);
	u_sendto_servers(NULL, ":%U MODE %U :+o", si->u, si->u);
	u_user_num(si->u, RPL_YOUREOPER);
	u_log(LG_VERBOSE, "%U now has user mode %s",
	      si->u, u_user_modes(si->u));

	return 0;
}

static u_cmd oper_cmdtab[] = {
	{ "OPER", SRC_LOCAL_USER, c_lu_oper, 2 },
	{ }
};

TETHYS_MODULE_V1(
	"core/oper", "Alex Iadicicco", "OPER command",
	NULL, NULL, oper_cmdtab);

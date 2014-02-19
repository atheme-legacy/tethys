/* Tethys, core/upgrade -- UPGRADE command
   Copyright (C) 2014 Alex Iadicicco
   Copyright (C) 2014 Hugo Landau

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_a_upgrade(u_sourceinfo *si, u_msg *msg)
{
	int rc;
	u_user_num(si->u, RPL_UPGRADESTARTING);
	begin_upgrade();
	u_user_num(si->u, RPL_UPGRADEFAILED);
	return 0;
}

static u_cmd upgrade_cmdtab[] = {
	{ "UPGRADE", SRC_LOCAL_OPER, c_a_upgrade, 0 },
	{ }
};

TETHYS_MODULE_V1(
	"core/upgrade", "Hugo Landau", "UPGRADE command",
	NULL, NULL, upgrade_cmdtab);
/* vim: set noet: */

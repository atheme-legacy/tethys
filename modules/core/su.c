/* Tethys, core/su -- ENCAP SU command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_es_su(u_sourceinfo *si, u_msg *msg)
{
	u_user *u;
	char *user = msg->argv[2];
	char *acct = msg->argc > 3 ? msg->argv[3] : "";

	if (!(u = u_user_by_uid(user))) {
		u_log(LG_WARN, "%I tried to ENCAP SU nonexistent %s", si, user);
		return 0;
	}

	u_strlcpy(u->acct, acct, MAXACCOUNT+1);

	if (*acct) {
		u_log(LG_VERBOSE, "%U logged in to %s", u, acct);
	} else {
		u_log(LG_VERBOSE, "%U logged out", u);
	}

	return 0;
}

static u_cmd su_cmdtab[] = {
	{ "SU", SRC_ENCAP_SERVER, c_es_su, 3 },
	{ }
};

TETHYS_MODULE_V1(
	"core/su", "Alex Iadicicco", "ENCAP SU command",
	NULL, NULL, su_cmdtab);

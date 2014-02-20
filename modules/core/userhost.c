/* Tethys, core/userhost -- USERHOST command
   Copyright (C) 2013 Elizabeth Myers

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_lu_userhost(u_sourceinfo *si, u_msg *msg)
{
	u_user *tu;
	int i, w, max;
	char buf[512], data[512];
	char *ptr = buf;

	max = 501 - strlen(me.name) - strlen(si->u->nick);
	buf[0] = '\0';

	/* TODO - last param could contain multiple targets */
	for (i=0; i<msg->argc; i++) {
		tu = u_user_by_nick(msg->argv[i]);
		if (tu == NULL)
			continue;

		w = snprintf(data, 512, "%s%s=%c%s@%s", tu->nick,
		             IS_OPER(tu) ? "*" : "",
		             IS_AWAY(tu) ? '-' : '+',
		             tu->ident, tu->host);

		if (ptr + w + 1 > buf + max) {
			u_src_num(si, RPL_USERHOST, buf);
			ptr = buf;
		}

		if (ptr != buf)
			*ptr++ = ' ';

		u_strlcpy(ptr, data, buf + max - ptr);
		ptr += w;
	}

	if (ptr != buf)
		u_src_num(si, RPL_USERHOST, buf);

	return 0;
}

static u_cmd userhost_cmdtab[] = {
	{ "USERHOST", SRC_LOCAL_USER, c_lu_userhost, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/userhost", "Elizabeth Myers", "USERHOST command",
	NULL, NULL, userhost_cmdtab);

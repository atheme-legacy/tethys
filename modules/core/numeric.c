/* Tethys, core/numeric -- Numeric routing
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* maybe this should be in util? */
static void irccat(char *buf, int sz, int argc, char **argv)
{
	int p = 0;
	bool last = false;

	sz--;

	for (; argc > 0 && p < sz && !last; argc--, argv++) {
		if (sz - p > 0)
			buf[p++] = ' ';

		if (strchr(*argv, ' ') && sz - p > 0) {
			buf[p++] = ':';
			last = true;
		}

		p += snprintf(buf + p, sz - p, "%s", *argv);
	}

	buf[p] = '\0';
}

static int c_s_num(u_sourceinfo *si, u_msg *msg)
{
	char *tgtid = msg->argv[0];
	int n, num;
	char buf[512];
	u_server *sv;
	u_user *u;

	irccat(buf, 512, msg->argc - 1, msg->argv + 1);

	num = atoi(msg->command);
	if (num < 0 || num > 1000) {
		u_log(LG_ERROR, "%I sent bad numeric %d", si, num);
		return 0;
	}
	if (num < 100)
		num += 100;

	n = strlen(tgtid);
	switch (n) {
	case 3:
		if (!(sv = u_server_by_sid(tgtid)))
			goto badtgt;
		u_link_f(sv->link, ":%I %03d %S%s", si, num, sv, buf);
		break;

	case 9:
		if (!(u = u_user_by_uid(tgtid)))
			goto badtgt;
		u_link_f(u->link, ":%I %03d %U%s", si, num, u, buf);
		break;

	default:
		goto badtgt;
	}

	return 0;

badtgt:
	u_log(LG_ERROR, "%I sent bad numeric target %s", si, tgtid);
	return 0;
}

static u_cmd numeric_cmdtab[] = {
	{ "###", SRC_SERVER, c_s_num, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/numeric", "Alex Iadicicco", "Numeric routing",
	NULL, NULL, numeric_cmdtab);

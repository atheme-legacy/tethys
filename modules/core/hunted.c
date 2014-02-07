/* Tethys, core/hunted -- Simple hunted commands
   Copyright (C) 2014 Alex Iadicicco

   This code is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* The commands in this file are all of the form "COMMAND [server]", where
   the optional server parameter is a "hunted" parameter. If a command has
   any other arguments, it should get its own implementation elsewhere. */

/* returns "true" to mean "you take care of it" */
static bool hunted(u_sourceinfo *si, u_msg *msg)
{
	u_server *sv;
	char *tgt = msg->argv[0];

	if (SRC_IS_LOCAL_USER(si)) {
		if (!tgt || !casecmp(tgt, me.name))
			return true;

		if (!(sv = u_server_by_name(tgt))) {
			u_src_num(si, ERR_NOSUCHSERVER, tgt);
			return false;
		}
	} else {
		if (!tgt || !casecmp(tgt, me.sid))
			return true;

		if (!(sv = u_server_by_sid(tgt))) {
			u_src_num(si, ERR_NOSUCHSERVER, tgt);
			return false;
		}
	}

	u_conn_f(sv->link, ":%I %s %S", si, msg->command, sv);
	return false;
}

/* TODO: allow servers to query VERSION as well */
static int c_u_version(u_sourceinfo *si, u_msg *msg)
{
	if (!hunted(si, msg))
		return 0;

	u_src_num(si, RPL_VERSION, PACKAGE_FULLNAME, me.name, revision,
	          PACKAGE_COPYRIGHT);
	u_user_send_isupport(si->u);

	return 0;
}

static int c_u_motd(u_sourceinfo *si, u_msg *msg)
{
	if (!hunted(si, msg))
		return 0;

	u_user_send_motd(si->u);

	return 0;
}

static int c_u_admin(u_sourceinfo *si, u_msg *msg)
{
	mowgli_node_t *n;

	if (!hunted(si, msg))
		return 0;

	u_src_num(si, RPL_ADMINME, &me);
	MOWGLI_ITER_FOREACH(n, my_admininfo.head)
		u_src_num(si, RPL_ADMINEMAIL, n->data);

	return 0;
}

static int c_u_summon(u_sourceinfo *si, u_msg *msg)
{
	u_src_num(si, ERR_SUMMONDISABLED);
	return 0;
}

static u_cmd hunted_cmdtab[] = {
	{ "VERSION",  SRC_USER, c_u_version,  0 },
	{ "MOTD",     SRC_USER, c_u_motd,     0 },
	{ "ADMIN",    SRC_USER, c_u_admin,    0 },
	{ "SUMMON",   SRC_USER, c_u_summon,   0 },
	{ }
};

TETHYS_MODULE_V1(
	"core/hunted", "Alex Iadicicco", "Simple hunted commands",
	NULL, NULL, hunted_cmdtab);

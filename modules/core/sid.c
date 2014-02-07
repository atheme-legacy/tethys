/* Tethys, core/sid -- SID and SERVER messages
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_s_sid(u_sourceinfo *si, u_msg *msg)
{
	u_server_new_remote(si->s,
	                    msg->argv[2],  /* sid */
	                    msg->argv[0],  /* name */
	                    msg->argv[3]); /* desc */

	msg->propagate = CMD_DO_BROADCAST;

	return 0;
}

static int c_s_server(u_sourceinfo *si, u_msg *msg)
{
	u_server_new_remote(si->s,
	                    NULL,          /* sid */
	                    msg->argv[0],  /* name */
	                    msg->argv[2]); /* desc */

	msg->propagate = CMD_DO_BROADCAST;

	return 0;
}

static u_cmd sid_cmdtab[] = {
	{ "SID",    SRC_SERVER,  c_s_sid,    4, U_RATELIMIT_NONE, CMD_PROP_BROADCAST },
	{ "SERVER", SRC_SERVER,  c_s_server, 3, U_RATELIMIT_NONE, CMD_PROP_BROADCAST },
	{ }
};

TETHYS_MODULE_V1(
	"core/sid", "Alex Iadicicco", "SID and SERVER messages",
	NULL, NULL, sid_cmdtab);

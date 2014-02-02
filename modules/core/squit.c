/* Tethys, core/squit -- SQUIT command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* 3 cases:
     - we are being squitted by si->source
     - we are to squit a local link
     - somebody else is getting squitted
 */
static int c_a_squit(u_sourceinfo *si, u_msg *msg)
{
	char *target = msg->argv[0];
	char *reason = msg->argv[1];
	u_server *sv;
	bool is_local;

	if (!(sv = u_server_by_ref(si->source, target))) {
		if (SRC_IS_LOCAL_USER(si)) {
			u_src_num(si, ERR_NOSUCHSERVER, target);
		} else {
			u_log(LG_ERROR, "%I tried to squit nonexistent %s",
			      si, target);
		}
		return 0;
	}

	is_local = false;
	if (sv == &me || sv->conn == si->source) {
		if (sv == &me && SRC_IS_LOCAL_USER(si)) {
			u_conn_f(si->source, ":%S NOTICE %U :%s",
				 &me, si->u, "you are trying to squit me");
			return 0;
		}

		sv = si->source->priv;
		is_local = true;
	}

	if (is_local) {
		reason = reason ? reason : "<No reason given>";
		u_sendto_servers(si->source, ":%S SQUIT %S :%s (%s)",
		                 &me, sv, si->name, reason);
	} else {
		char *s = reason ? " :" : "";
		reason = reason ? reason : "";

		u_sendto_servers(si->source, ":%I SQUIT %S%s%s",
		                 si, sv, s, reason);
	}

	u_server_unlink(sv);

	return 0;
}

static u_cmd squit_cmdtab[] = {
	{ "SQUIT", SRC_S2S | SRC_LOCAL_OPER,  c_a_squit, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/squit", "Alex Iadicicco", "SQUIT command",
	NULL, NULL, squit_cmdtab);

/* Tethys, core/module -- Module management commands
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* Someday this module should be made hunted */

static int c_lo_module(u_sourceinfo *si, u_msg *msg)
{
	char *subcmd;
	char *module = msg->argv[0];
	char *op;
	bool success;

	ascii_canonize(msg->command);
	subcmd = msg->command + 3;

	if (streq(subcmd, "LOAD")) {
		op = "";
		success = u_module_load(module);
	} else if (streq(subcmd, "UNLOAD")) {
		op = "un";
		success = u_module_unload(module);
	} else if (streq(subcmd, "RELOAD")) {
		op = "re";
		success = u_module_reload_or_load(module);
	} else {
		u_src_num(si, ERR_UNKNOWNCOMMAND, msg->command);
		return 0;
	}

	u_conn_f(si->link,
	         success
	            ? ":%S NOTICE %U :\2%s\2 %sloaded successfully"
	            : ":%S NOTICE %U :\2%s\2 failed to %sload",
	         &me, si->u, module, op);

	return 0;
}

static int c_o_modlist(u_sourceinfo *si, u_msg *msg)
{
	mowgli_patricia_iteration_state_t state;
	u_module *m;

	if (msg->argc > 0) {
		u_server *sv = u_server_by_name(msg->argv[0]);

		if (sv == NULL) {
			u_src_num(si, ERR_NOSUCHSERVER, msg->argv[0]);
			return 0;
		}

		if (sv != &me) {
			u_conn_f(sv->conn, ":%I MODLIST %s", si, sv->name);
			return 0;
		}
	}

	u_conn_f(si->link, ":%S NOTICE %U :Loaded modules:", &me, si->u);

	MOWGLI_PATRICIA_FOREACH(m, &state, u_modules) {
		u_conn_f(si->link, ":%S NOTICE %U : - \2%s\2 (%s)", &me,
			 si->u, m->info->name, m->info->description);
	}

	return 0;
}

static u_cmd module_cmdtab[] = {
	{ "MODLOAD",    SRC_LOCAL_OPER,  c_lo_module, 1 },
	{ "MODUNLOAD",  SRC_LOCAL_OPER,  c_lo_module, 1 },
	{ "MODRELOAD",  SRC_LOCAL_OPER,  c_lo_module, 1 },
	{ "MODLIST",    SRC_OPER,        c_o_modlist, 0 },
	{ }
};

static int module_init(u_module *m)
{
	m->flags |= MODULE_PERMANENT;
}

TETHYS_MODULE_V1(
	"core/module", "Alex Iadicicco", "Module management commands",
	module_init, NULL, module_cmdtab);

/* ircd-micro, core/c_42.c -- Example module
   Copyright (C) 2014 Sam Dodrill

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int m_42(u_conn *conn, u_msg *msg)
{
	u_user *u = conn->priv;
	u_conn_f(conn, ":%S NOTICE %U :The Answer to Life, the Universe, and %s",
			&me, u, (u->flags & UMODE_OPER) ? "matthew" : "Everything");
	return 0;
}

u_cmd c_42[] = {
	{"42",      CTX_USER, m_42, 0},
	{ "" },
};

int c_42_init(u_module *m)
{
	u_log(LG_DEBUG, "%s initalizing", m->info->name);
	u_cmds_reg(c_42);
}

void c_42_deinit(u_module *m)
{
	u_log(LG_DEBUG, "%s deinitializing", m->info->name);
}

MICRO_MODULE_V1(
	"core/c_42", "Sam Dodrill",
	"Example command module",

	c_42_init,
	c_42_deinit
);

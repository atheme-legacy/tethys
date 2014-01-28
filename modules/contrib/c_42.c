/* Tethys, core/c_42.c -- Provides /42
   Copyright (C) 2014 Sam Dodrill

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int m_42(u_sourceinfo *si, u_msg *msg)
{
	u_conn_f(si->link, ":%S NOTICE %U :The Answer to Life, the Universe, and %s",
	         &me, si->u, (si->u->flags & UMODE_OPER) ? "matthew" : "Everything");
	return 0;
}

u_cmd c_42 = {"42", SRC_LOCAL_USER, m_42, 0};

int c_42_init(u_module *m)
{
	u_cmd_reg(&c_42);
}

TETHYS_MODULE_V1(
	"contrib/c_42", "Sam Dodrill",
	"Provides /42",

	c_42_init,
	NULL,

	NULL
);

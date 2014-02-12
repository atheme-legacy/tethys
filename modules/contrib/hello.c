/* Tethys, core/hello.c -- Example module
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

int hello_init(u_module *m)
{
	u_log(LG_DEBUG, "%s initalizing", m->info->name);
	return 0;
}

void hello_deinit(u_module *m)
{
	u_log(LG_DEBUG, "%s deinitializing", m->info->name);
}

TETHYS_MODULE_V1(
	"contrib/hello", "Alex Iadicicco",
	"Example module",

	hello_init,
	hello_deinit,

	NULL
);

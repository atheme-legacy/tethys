/* Tethys, core/mkpass -- MKPASS command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_lu_mkpass(u_sourceinfo *si, u_msg *msg)
{
	char buf[CRYPTLEN], salt[CRYPTLEN];

	u_crypto_gen_salt(salt);
	u_crypto_hash(buf, msg->argv[0], salt);

	u_conn_f(si->link, ":%S NOTICE %U :%s", &me, si->u, buf);

	return 0;
}

static u_cmd mkpass_cmdtab[] = {
	{ "MKPASS", SRC_LOCAL_USER, c_lu_mkpass, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/mkpass", "Alex Iadicicco", "MKPASS command",
	NULL, NULL, mkpass_cmdtab);

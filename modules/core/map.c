/* Tethys, core/map -- MAP command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

struct map_priv {
	char indent[512], buf[512];
	u_user *u;

	u_server *sv;
	int depth, left;
};

static void do_map(struct map_priv *p)
{
	int len, left, depth = p->depth << 2;
	mowgli_patricia_iteration_state_t state;
	u_server *tsv, *sv = p->sv;

	p->indent[depth] = '\0';
	if (depth != 0) {
		p->left--;
		p->indent[depth - 3] = p->left ? '|' : '`';
		p->indent[depth - 2] = '-';
	}

	len = snf(FMT_USER, p->buf, 512, "%s%s[%s] ",
	          p->indent, sv->name, sv->sid[0] ? sv->sid : "JUPE");
	memset(p->buf + len, '-', 512 - len);
	snf(FMT_USER, p->buf + 50, 462, " | Users: %5d", sv->nusers);

	/* send! */
	u_user_num(p->u, RPL_MAP, p->buf);

	p->indent[depth] = ' ';
	if (depth != 0) {
		p->indent[depth - 2] = ' ';
		if (p->left == 0)
			p->indent[depth - 3] = ' ';
	}

	if (sv->nlinks > 0) {
		left = p->left;
		p->left = sv->nlinks;
		p->depth = (depth >> 2) + 1;
		MOWGLI_PATRICIA_FOREACH(tsv, &state, servers_by_name) {
			if (tsv->parent != sv)
				continue;
			p->sv = tsv;
			do_map(p);
			p->sv = sv;
		}
		p->depth = (depth >> 2);
		p->left = left;
	}

	if (depth != 0) {
		p->indent[depth - 3] = ' ';
		p->indent[depth - 2] = ' ';
	}
}

static int c_lo_map(u_sourceinfo *si, u_msg *msg)
{
	struct map_priv p;

	memset(p.indent, ' ', 512);
	p.sv = &me;
	p.depth = 0;
	p.u = si->u;
	p.left = 0;
	do_map(&p);
	u_user_num(si->u, RPL_MAPEND);

	return 0;
}

struct u_cmd map_cmdtab[] = {
	{ "MAP", SRC_LOCAL_OPER, c_lo_map, 0 },
	{ }
};

TETHYS_MODULE_V1(
	"core/map", "Alex Iadicicco", "MAP command",
	NULL, NULL, map_cmdtab);

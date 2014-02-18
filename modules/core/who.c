/* Tethys, core/who -- WHO command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void who_reply(u_sourceinfo *si, u_user *u, u_chan *c, u_chanuser *cu)
{
	u_server *sv;
	char *s, buf[6];
	mowgli_node_t *n;

	if (c == NULL) {
		/* oh god this is so bad */
		u_map_each_state state;
		U_MAP_EACH(&state, u->channels, &c, NULL)
			break;
		cu = NULL;
	}

	if (c != NULL && cu == NULL)
		cu = u_chan_user_find(c, u);

	if (cu == NULL) /* this is an error */
		c = NULL;

	s = buf;
	*s++ = u->away[0] ? 'G' : 'H';
	if (u->mode & UMODE_OPER)
		*s++ = '*';
	MOWGLI_LIST_FOREACH(n, cu_pfx_list.head) {
		u_cu_pfx *cs = n->data;
		if (cu && (cu->flags & cs->mask))
			*s++ = cs->prefix;
	}
	*s = '\0';

	u_src_num(si, RPL_WHOREPLY, c, u->ident, u->host, u->sv->name,
	          u->nick, buf, 0, u->gecos);
}

static int c_lu_who(u_sourceinfo *si, u_msg *msg)
{
	u_user *u;
	u_chan *c = NULL;
	u_chanuser *cu;
	char *name = msg->argv[0];

	if (strchr(CHANTYPES, *name)) {
		u_map_each_state state;
		bool visible_only = false;

		if ((c = u_chan_get(name)) == NULL)
			goto end;

		if ((cu = u_chan_user_find(c, si->u)) == NULL) {
			if (c->mode & CMODE_SECRET)
				goto end;
			visible_only = true;
		}

		U_MAP_EACH(&state, c->members, &u, &cu) {
			if (visible_only && (u->mode & UMODE_INVISIBLE))
				continue;
			who_reply(si, u, c, cu);
		}
	} else {
		if ((u = u_user_by_nick(name)) == NULL)
			goto end;

		who_reply(si, u, NULL, NULL);
	}

end:
	u_src_num(si, RPL_ENDOFWHO, name);
	return 0;
}

static u_cmd who_cmdtab[] = {
	{ "WHO", SRC_LOCAL_USER, c_lu_who, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/who", "Alex Iadicicco", "WHO command",
	NULL, NULL, who_cmdtab);

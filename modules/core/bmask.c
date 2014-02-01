/* Tethys, core/bmask -- BMASK command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static bool get_bmask_list(char type, u_chan *c, mowgli_list_t **list)
{
	switch (type) {
	case 'b':
		*list = &c->ban;
		break;
	case 'e':
		*list = &c->banex;
		break;
	case 'I':
		*list = &c->invex;
		break;
	case 'q':
		*list = &c->quiet;
		break;
	default:
		return false;
	}

	return true;
}

static void apply_bmask(u_sourceinfo *si, u_chan *c, char type,
                        mowgli_list_t *list, char *mask)
{
	mowgli_node_t *n;
	u_chanban *ban;

	/* there's GOT to be a better way! */
	MOWGLI_LIST_FOREACH(n, list->head) {
		ban = n->data;
		if (streq(ban->mask, mask)) {
			u_log(LG_DEBUG, "apply_bmask: skipping %s: %s",
			      mask, "already in list");
			return;
		}
	}

	ban = malloc(sizeof(*ban));
	u_strlcpy(ban->mask, mask, 256);
	snf(FMT_USER, ban->setter, 256, "%I", si);
	ban->time = NOW.tv_sec;
	mowgli_node_add(ban, &ban->n, list);

	u_sendto_chan(c, NULL, ST_USERS, ":%I MODE %C +%c %s",
	              si, c, type, mask);
	u_log(LG_DEBUG, "apply_bmask: %C +%c %s", c, type, mask);
}

static int c_s_bmask(u_sourceinfo *si, u_msg *msg)
{
	u_chan *c;
	int chants = atoi(msg->argv[0]);
	char *channame = msg->argv[1];
	char type = *msg->argv[2];
	char *ban, *bans = msg->argv[3];
	u_strop_state st;
	mowgli_list_t *list;

	if ((c = u_chan_get(channame))) {
		if (chants > c->ts)
			return 0;
	}

	msg->propagate = CMD_DO_BROADCAST;
	if (c == NULL)
		c = u_chan_create(channame);

	if (!get_bmask_list(type, c, &list)) {
		u_log(LG_WARN, "%I sent BMASK for unk. type %c", si, type);
		return 0;
	}

	U_STROP_SPLIT(&st, bans, " ", &ban)
		apply_bmask(si, c, type, list, ban);

	return 0;
}

static u_cmd bmask_cmdtab[] = {
	{ "BMASK", SRC_SERVER, c_s_bmask, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/bmask", "Alex Iadicicco", "BMASK command",
	NULL, NULL, bmask_cmdtab);

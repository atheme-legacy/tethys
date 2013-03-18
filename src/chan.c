/* ircd-micro, chan.c -- channels
   Copyright (c) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int cb_flag();
static int cb_list();
static int cb_string();
static int cb_prefix();

static struct u_cmode_info __cmodes[32] = {
	{ 'p', cb_flag,   CMODE_PRIVATE                     },
	{ 's', cb_flag,   CMODE_SECRET                      },
	{ 'i', cb_flag,   CMODE_INVITEONLY                  },
	{ 't', cb_flag,   CMODE_TOPIC                       },
	{ 'n', cb_flag,   CMODE_NOEXTERNAL                  },
	{ 'm', cb_flag,   CMODE_MODERATED                   },
	{ 'z', cb_flag,   CMODE_OPMOD                       },
	{ 'c', cb_flag,   CMODE_NOCOLOR                     },
	{ 'g', cb_flag,   CMODE_FREEINVITE                  },
	{ 'f', cb_string, offsetof(struct u_chan, forward)  },
	{ 'k', cb_string, offsetof(struct u_chan, key)      },
	{ 'b', cb_list,   offsetof(struct u_chan, ban)      },
	{ 'q', cb_list,   offsetof(struct u_chan, quiet)    },
	{ 'e', cb_list,   offsetof(struct u_chan, banex)    },
	{ 'I', cb_list,   offsetof(struct u_chan, invex)    },
	{ 'o', cb_prefix, CU_PFX_OP                         },
	{ 'v', cb_prefix, CU_PFX_VOICE                      },
};

int cb_flag(info, c, on, getarg)
struct u_cmode_info *info;
struct u_chan *c;
char *(*getarg)();
{
	if (on)
		c->mode |= info->data;
	else
		c->mode &= ~info->data;

	return 0;
}

int cb_list(info, c, on, getarg)
struct u_cmode_info *info;
struct u_chan *c;
char *(*getarg)();
{
	struct u_list *list, *n;
	char *ban, *arg = getarg();

	if (arg == NULL)
		return -1;

	list = member(struct u_list*, c, info->data);

	U_LIST_EACH(n, list) {
		ban = n->data;
		if (!strcmp(ban, arg)) {
			if (!on)
				free(u_list_del_n(n));
			return 0;
		}
	}

	if (on)
		u_list_add(list, u_strdup(ban));

	return 0;
}

int cb_string(info, c, on, getarg)
struct u_cmode_info *info;
struct u_chan *c;
char *(*getarg)();
{
	char *arg, **val;

	val = &member(char*, c, info->data);

	if (!on) {
		free(*val);
		*val = NULL;
	} else {
		arg = getarg();
		if (arg == NULL)
			return -1;
		if (*val)
			free(*val);
		*val = u_strdup(arg);
	}

	return 0;
}

int cb_prefix(info, c, on, getarg)
struct u_cmode_info *info;
struct u_chan *c;
char *(*getarg)();
{
	struct u_chanuser *cu;
	struct u_user *u;
	char *arg = getarg();

	if (arg == NULL)
		return -1;
	u = u_user_by_nick(arg);
	if (u == NULL) {
		/* TODO: u_user_by_nick_history */
		return -1;
	}
	cu = u_chan_user(c, u);
	if (cu == NULL)
		return -1;

	if (on)
		cu->flags |= info->data;
	else
		cu->flags &= ~info->data;
}

struct u_chanuser *u_chan_user(c, u)
struct u_chan *c;
struct u_user *u;
{
	struct u_list *n;
	struct u_chanuser *cu;
	/* woo, more linear search! */
	U_LIST_EACH(n, &c->members) {
		cu = n->data;
		if (cu->u == u)
			return cu;
	}
	return NULL;
}

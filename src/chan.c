/* ircd-micro, chan.c -- channels
   Copyright (c) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static struct u_trie *all_chans;

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

struct u_cmode_info *cmodes = __cmodes;
unsigned cmode_default = CMODE_TOPIC | CMODE_NOEXTERNAL;

static int cb_flag(info, c, on, getarg)
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

static int cb_list(info, c, on, getarg)
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

static int cb_string(info, c, on, getarg)
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

static int cb_prefix(info, c, on, getarg)
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
	cu = u_chan_user_find(c, u);
	if (cu == NULL)
		return -1;

	if (on)
		cu->flags |= info->data;
	else
		cu->flags &= ~info->data;

	return 0;
}

struct u_chan *u_chan_get(name)
char *name;
{
	return u_trie_get(all_chans, name);
}

struct u_chan *u_chan_get_or_create(name)
char *name;
{
	struct u_chan *chan;

	chan = u_chan_get(name);
	if (chan != NULL)
		return chan;

	chan = malloc(sizeof(*chan));
	u_strlcpy(chan->name, name, MAXCHANNAME+1);
	chan->mode = cmode_default;
	u_cookie_inc(&chan->ck_flags);
	chan->members = u_map_new();
	u_list_init(&chan->ban);
	u_list_init(&chan->quiet);
	u_list_init(&chan->banex);
	u_list_init(&chan->invex);
	chan->forward = NULL;
	chan->key = NULL;

	u_trie_set(all_chans, chan->name, chan);

	return chan;
}

static void drop_list(list)
struct u_list *list;
{
	struct u_list *n, *tn;

	U_LIST_EACH_SAFE(n, tn, list) {
		free(n->data);
		u_list_del_n(n);
	}
}

static void drop_param(p)
char **p;
{
	if (*p != NULL)
		free(*p);
	*p = NULL;
}

void u_chan_drop(chan)
struct u_chan *chan;
{
	/* TODO: u_map_free callback! */
	u_map_free(chan->members);
	drop_list(&chan->ban);
	drop_list(&chan->quiet);
	drop_list(&chan->banex);
	drop_list(&chan->invex);
	drop_param(&chan->forward);
	drop_param(&chan->key);

	u_trie_del(all_chans, chan->name);
	free(chan);
}

/* XXX: assumes the chanuser doesn't already exist */
struct u_chanuser *u_chan_user_add(c, u)
struct u_chan *c;
struct u_user *u;
{
	struct u_chanuser *cu;

	cu = malloc(sizeof(*cu));
	cu->flags = 0;
	u_cookie_reset(&cu->ck_flags);
	cu->c = c;
	cu->u = u;

	u_map_set(c->members, u, cu);

	return cu;
}

void u_chan_user_del(cu)
struct u_chanuser *cu;
{
	u_map_del(cu->c->members, cu->u);
	free(cu);
}

struct u_chanuser *u_chan_user_find(c, u)
struct u_chan *c;
struct u_user *u;
{
	return u_map_get(c->members, u);
}

int init_chan()
{
	all_chans = u_trie_new(ascii_canonize);
	return all_chans != NULL ? 0 : -1;
}

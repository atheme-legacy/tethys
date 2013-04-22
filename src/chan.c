/* ircd-micro, chan.c -- channels
   Copyright (c) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

u_trie *all_chans;

static void cb_flag();
static void cb_list();
static void cb_string();
static void cb_prefix();

static u_cmode_info __cmodes[] = {
	{ 'c', cb_flag,   CMODE_NOCOLOR              },
	{ 'g', cb_flag,   CMODE_FREEINVITE           },
	{ 'i', cb_flag,   CMODE_INVITEONLY           },
	{ 'm', cb_flag,   CMODE_MODERATED            },
	{ 'n', cb_flag,   CMODE_NOEXTERNAL           },
	{ 'p', cb_flag,   CMODE_PRIVATE              },
	{ 's', cb_flag,   CMODE_SECRET               },
	{ 't', cb_flag,   CMODE_TOPIC                },
	{ 'z', cb_flag,   CMODE_OPMOD                },
	{ 'f', cb_string, offsetof(u_chan, forward)  },
	{ 'k', cb_string, offsetof(u_chan, key)      },
	{ 'b', cb_list,   offsetof(u_chan, ban)      },
	{ 'q', cb_list,   offsetof(u_chan, quiet)    },
	{ 'e', cb_list,   offsetof(u_chan, banex)    },
	{ 'I', cb_list,   offsetof(u_chan, invex)    },
	{ 'o', cb_prefix, CU_PFX_OP                  },
	{ 'v', cb_prefix, CU_PFX_VOICE               },
	{ 0 }
};

u_cmode_info *cmodes = __cmodes;
uint cmode_default = CMODE_TOPIC | CMODE_NOEXTERNAL;

static int cm_on;
static char *cm_buf_p, cm_buf[128];
static char *cm_data_p, cm_data[512];

void u_chan_m_start(u, c) u_user *u; u_chan *c;
{
	u_cookie_inc(&c->ck_flags);

	cm_on = -1;
	cm_buf_p = cm_buf;
	cm_data_p = cm_data;
}

void u_chan_m_end(u, c) u_user *u; u_chan *c;
{
	*cm_buf_p = '\0';
	*cm_data_p = '\0';

	u_sendto_chan(c, NULL, ":%H MODE %C %s%s", u, c, cm_buf, cm_data);
}

static void cm_put(on, ch, arg) char ch, *arg;
{
	if (on != cm_on) {
		cm_on = on;
		*cm_buf_p++ = on ? '+' : '-';
	}
	*cm_buf_p++ = ch;
	if (arg != NULL)
		cm_data_p += sprintf(cm_data_p, " %s", arg);
}

static void cb_flag(info, c, u, on, getarg)
u_cmode_info *info; u_chan *c; u_user *u; char *(*getarg)();
{
	uint oldm = c->mode;
	if (on)
		c->mode |= info->data;
	else
		c->mode &= ~info->data;
	if (oldm != c->mode)
		cm_put(on, info->ch, NULL);
	return;
}

static void cb_list(info, c, u, on, getarg)
u_cmode_info *info; u_chan *c; u_user *u; char *(*getarg)();
{
	u_list *list, *n;
	u_chanban *ban;
	char *arg = getarg();

	if (arg == NULL) {
		/* TODO: send list in this case */
		return;
	}

	list = (u_list*)memberp(c, info->data);

	U_LIST_EACH(n, list) {
		ban = n->data;
		if (!strcmp(ban->mask, arg)) {
			if (!on) {
				cm_put(on, info->ch, ban->mask);
				free(u_list_del_n(list, n));
			}
			return;
		}
	}

	if (on) {
		ban = malloc(sizeof(*ban));

		u_strlcpy(ban->mask, arg, 256);
		snf(FMT_USER, ban->setter, 256, "%H", u);
		ban->time = NOW.tv_sec;

		cm_put(on, info->ch, arg);
		u_list_add(list, ban);
	}

	return;
}

static void cb_string(info, c, u, on, getarg)
u_cmode_info *info; u_chan *c; u_user *u; char *(*getarg)();
{
	char *arg, **val;

	val = (char**)memberp(c, info->data);

	if (!on) {
		free(*val);
		*val = NULL;
		cm_put(on, info->ch, NULL);
	} else {
		arg = getarg();
		if (arg == NULL)
			return;
		if (*val)
			free(*val);
		*val = u_strdup(arg);
		cm_put(on, info->ch, arg);
	}
}

static void cb_prefix(info, c, u, on, getarg)
u_cmode_info *info; u_chan *c; u_user *u; char *(*getarg)();
{
	u_chanuser *cu;
	u_user *tu;
	char *arg = getarg();

	if (arg == NULL)
		return;

	tu = u_user_by_nick(arg);
	if (tu == NULL) {
		/* TODO: u_user_by_nick_history */
		u_user_num(u, ERR_NOSUCHNICK, arg);
		return;
	}

	cu = u_chan_user_find(c, tu);
	if (cu == NULL) {
		u_user_num(u, ERR_USERNOTINCHANNEL, tu, c);
		return;
	}

	if (on)
		cu->flags |= info->data;
	else
		cu->flags &= ~info->data;
	cm_put(on, info->ch, tu->nick);
}

u_chan *u_chan_get(name) char *name;
{
	return u_trie_get(all_chans, name);
}

u_chan *u_chan_get_or_create(name) char *name;
{
	u_chan *chan;

	chan = u_chan_get(name);
	if (chan != NULL)
		return chan;

	chan = malloc(sizeof(*chan));
	u_strlcpy(chan->name, name, MAXCHANNAME+1);
	chan->topic[0] = '\0';
	chan->topic_setter[0] = '\0';
	chan->topic_time = 0;
	chan->mode = cmode_default;
	u_cookie_reset(&chan->ck_flags);
	chan->members = u_map_new(0);
	u_list_init(&chan->ban);
	u_list_init(&chan->quiet);
	u_list_init(&chan->banex);
	u_list_init(&chan->invex);
	chan->forward = NULL;
	chan->key = NULL;

	u_trie_set(all_chans, chan->name, chan);

	return chan;
}

static void drop_list(list) u_list *list;
{
	u_list *n, *tn;

	U_LIST_EACH_SAFE(n, tn, list) {
		free(n->data);
		u_list_del_n(list, n);
	}
}

static void drop_param(p) char **p;
{
	if (*p != NULL)
		free(*p);
	*p = NULL;
}

void u_chan_drop(chan) u_chan *chan;
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

char *u_chan_modes(c) u_chan *c;
{
	static char buf[64];
	u_cmode_info *info;
	char *s = buf;

	*s++ = '+';
	for (info=cmodes; info->ch; info++) {
		if (info->cb == cb_flag && (c->mode & info->data))
			*s++ = info->ch;
	}
	*s = '\0';

	return buf;
}

void u_chan_mode(c, u, ch, on, getarg)
u_chan *c; u_user *u; char ch; char *(*getarg)();
{
	u_cmode_info *info = cmodes;

	while (info->ch && info->ch != ch)
		info++;

	if (!info->ch) {
		u_user_num(u, ERR_UNKNOWNMODE, ch);
		return;
	}

	info->cb(info, c, u, on, getarg);
}

void u_chan_send_topic(c, u) u_chan *c; u_user *u;
{
	if (c->topic[0]) {
		u_user_num(u, RPL_TOPIC, c, c->topic);
		u_user_num(u, RPL_TOPICWHOTIME, c, c->topic_setter,
		           c->topic_time);
	} else {
		u_user_num(u, RPL_NOTOPIC, c);
	}
}

struct send_names_priv {
	u_chan *c;
	u_user *u;
	char pfx, *s, buf[512];
	uint w;
};

void send_names_cb(map, u, cu, priv)
u_map *map; u_user *u; u_chanuser *cu; struct send_names_priv *priv;
{
	char *p, nbuf[MAXNICKLEN+3];
	int retrying = 0;

	p = nbuf;
	if (cu->flags & CU_PFX_OP)
		*p++ = '@';
	if ((cu->flags & CU_PFX_VOICE)
	    && (p == nbuf || (priv->u->flags & CAP_MULTI_PREFIX)))
		*p++ = '+';
	strcpy(p, u->nick);

try_again:
	if (!wrap(priv->buf, &priv->s, priv->w, nbuf)) {
		if (retrying) {
			u_log(LG_SEVERE, "Can't fit %s into RPL_NAMREPLY!", nbuf);
			return;
		}
		u_user_num(priv->u, RPL_NAMREPLY, priv->pfx, priv->c, priv->buf);
		retrying = 1;
		goto try_again;
	}
}

/* :my.name 353 nick = #chan :...
   *       *****    ***     **  = 11 */
void u_chan_send_names(c, u) u_chan *c; u_user *u;
{
	struct send_names_priv priv;

	priv.c = c;
	priv.u = u;
	priv.pfx = (c->mode&CMODE_PRIVATE)?'*':((c->mode&CMODE_SECRET)?'@':'=');
	priv.s = priv.buf;
	priv.w = 512 - (strlen(me.name) + strlen(u->nick) + strlen(c->name) + 11);

	u_map_each(c->members, send_names_cb, &priv);
	if (priv.s != priv.buf)
		u_user_num(u, RPL_NAMREPLY, priv.pfx, c, priv.buf);
	u_user_num(u, RPL_ENDOFNAMES, c);
}

/* XXX: assumes the chanuser doesn't already exist */
u_chanuser *u_chan_user_add(c, u) u_chan *c; u_user *u;
{
	u_chanuser *cu;

	cu = malloc(sizeof(*cu));
	cu->flags = 0;
	u_cookie_reset(&cu->ck_flags);
	cu->c = c;
	cu->u = u;

	u_map_set(c->members, u, cu);
	u_map_set(u->channels, c, cu);

	return cu;
}

void u_chan_user_del(cu) u_chanuser *cu;
{
	u_map_del(cu->c->members, cu->u);
	u_map_del(cu->u->channels, cu->c);
	free(cu);
}

u_chanuser *u_chan_user_find(c, u) u_chan *c; u_user *u;
{
	return u_map_get(c->members, u);
}

int u_is_muted(cu) u_chanuser *cu;
{
	if (u_cookie_cmp(&cu->ck_flags, &cu->c->ck_flags) >= 0)
		return cu->flags & CU_MUTED;

	u_cookie_cpy(&cu->ck_flags, &cu->c->ck_flags);
	cu->flags &= ~CU_MUTED;

	if (cu->flags & (CU_PFX_OP | CU_PFX_VOICE))
		return 0;

	if (!u_user_in_list(cu->u, &cu->c->quiet)
	    && !(cu->c->mode & CMODE_MODERATED))
		return 0;

	cu->flags |= CU_MUTED;
	return CU_MUTED; /* not 1, to mimic cu->flags & CU_MUTED */
}

int init_chan()
{
	all_chans = u_trie_new(ascii_canonize);
	return all_chans != NULL ? 0 : -1;
}

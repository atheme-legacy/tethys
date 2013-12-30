/* ircd-micro, chan.c -- channels
   Copyright (c) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

u_trie *all_chans;

static int cb_flag(u_modes*, int, char*);
static int cb_list(u_modes*, int, char*);
static int cb_prefix(u_modes*, int, char*);
static int cb_fwd(u_modes*, int, char*);
static int cb_key(u_modes*, int, char*);
static int cb_limit(u_modes*, int, char*);

static u_mode_info __cmodes[] = {
	{ 'c', cb_flag,   CMODE_NOCOLOR              },
	{ 'g', cb_flag,   CMODE_FREEINVITE           },
	{ 'i', cb_flag,   CMODE_INVITEONLY           },
	{ 'm', cb_flag,   CMODE_MODERATED            },
	{ 'n', cb_flag,   CMODE_NOEXTERNAL           },
	{ 'p', cb_flag,   CMODE_PRIVATE              },
	{ 's', cb_flag,   CMODE_SECRET               },
	{ 't', cb_flag,   CMODE_TOPIC                },
	{ 'z', cb_flag,   CMODE_OPMOD                },
	{ 'f', cb_fwd,                               },
	{ 'k', cb_key,                               },
	{ 'l', cb_limit,                             },
	{ 'b', cb_list,   offsetof(u_chan, ban)      },
	{ 'q', cb_list,   offsetof(u_chan, quiet)    },
	{ 'e', cb_list,   offsetof(u_chan, banex)    },
	{ 'I', cb_list,   offsetof(u_chan, invex)    },
	{ 'o', cb_prefix, CU_PFX_OP                  },
	{ 'v', cb_prefix, CU_PFX_VOICE               },
	{ 0 }
};

u_mode_info *cmodes = __cmodes;
uint cmode_default = CMODE_TOPIC | CMODE_NOEXTERNAL;

static int cm_deny(u_modes *m)
{
	u_chanuser *cu;

	if (m->perms == &me) /* dirty but clever but dirty */
		return 0;

	cu = m->perms;
	if ((m->flags & CM_DENY) || !cu || !(cu->flags & CU_PFX_OP))
		m->flags |= CM_DENY;
	return m->flags & CM_DENY;
}

static int cb_flag(u_modes *m, int on, char *arg)
{
	u_chan *c = m->target;
	uint oldm = c->mode;

	if (cm_deny(m))
		return 0;

	if (on)
		c->mode |= m->info->data;
	else
		c->mode &= ~m->info->data;

	if (oldm != c->mode)
		u_mode_put(m, on, m->info->ch, NULL, NULL);

	return 0;
}

/* foo -> foo!*@*, aji@ -> *!aji@*, etc */
static char *full_hostmask(char *mask)
{
	static char buf[512];
	char *nick, *ident, *host, *ex, *at;

	if (*mask == '$') {
		/* extbans are untouched */
		strcpy(buf, mask);
		return buf;
	}

	nick = ident = host = "*";

	ex = strchr(mask, '!');
	at = strchr(mask, '@');

	if (ex) *ex++ = '\0';
	if (at) *at++ = '\0';

	if (!ex && !at) { /* foo */
		nick = mask;
	} else if (!ex) { /* foo@bar, @bar, foo@ */
		ident = mask;
		host = at;
	} else if (!at) { /* aji!ex, aji!, !ex */
		nick = mask;
		ident = ex;
	} else { /* aji!i@host, !@host, etc. */
		nick = mask;
		ident = ex;
		host = at;
	}

	if (!*nick) nick = "*";
	if (!*ident) ident = "*";
	if (!*host) host = "*";

	sprintf(buf, "%s!%s@%s", nick, ident, host);
	return buf;
}

static int cb_list(u_modes *m, int on, char *arg)
{
	u_chan *c = m->target;
	u_user *u = m->setter;
	u_list *list, *n;
	u_chanban *ban;
	char *mask;

	if (arg == NULL) {
		if (on && (!strchr("eI", m->info->ch) || !cm_deny(m)))
			u_mode_put_l(m, m->info->ch);
		return 0;
	}

	if (cm_deny(m))
		return 1;

	list = (u_list*)memberp(c, m->info->data);
	mask = full_hostmask(arg);

	U_LIST_EACH(n, list) {
		ban = n->data;
		if (streq(ban->mask, mask)) {
			if (!on) {
				u_mode_put(m, on, m->info->ch, " %s", ban->mask);
				free(u_list_del_n(list, n));
			}
			return 1;
		}
	}

	if (on) {
		if (u_list_size(list) >= MAXBANLIST) {
			u_log(LG_VERBOSE, "%C +%c full, not adding %s",
		              c, m->info->ch, mask);
			u_user_num(u, ERR_BANLISTFULL, c, mask);
			return 1;
		}

		ban = malloc(sizeof(*ban));

		u_strlcpy(ban->mask, mask, 256);
		snf(FMT_USER, ban->setter, 256, "%H", u);
		ban->time = NOW.tv_sec;

		u_mode_put(m, on, m->info->ch, " %s", mask);
		u_list_add(list, ban);
	}

	return 1;
}

static int cb_prefix(u_modes *m, int on, char *arg)
{
	u_chan *c = m->target;
	u_user *u = m->setter;
	u_chanuser *cu;
	u_user *tu;

	if (cm_deny(m) || arg == NULL)
		return 1;

	if (isdigit(arg[0])) {
		tu = u_user_by_uid(arg);
	} else {
		tu = u_user_by_nick(arg);
		if (tu == NULL) {
			/* TODO: u_user_by_nick_history */
			u_user_num(u, ERR_NOSUCHNICK, arg);
			return 1;
		}
	}
	if (tu == NULL)
		return 1;

	cu = u_chan_user_find(c, tu);
	if (cu == NULL) {
		u_user_num(u, ERR_USERNOTINCHANNEL, tu, c);
		return 1;
	}

	if (on)
		cu->flags |= m->info->data;
	else
		cu->flags &= ~m->info->data;
	u_mode_put(m, on, m->info->ch, " %U", tu);

	return 1;
}

static int cb_fwd(u_modes *m, int on, char *arg)
{
	u_user *u = m->setter;
	u_chan *tc, *c = m->target;
	u_chanuser *tcu;

	if (cm_deny(m))
		return on;

	if (!on) {
		if (c->forward) {
			free(c->forward);
			c->forward = NULL;
			u_mode_put(m, on, m->info->ch, NULL, NULL);
		}
		return 0;
	}

	if (arg == NULL)
		return 0;

	if (!(tc = u_chan_get(arg))) {
		u_user_num(u, ERR_NOSUCHCHANNEL, arg);
		return 1;
	}

	tcu = u_chan_user_find(tc, u);
	/* TODO: +F */
	if (tcu == NULL || !(tcu->flags & CU_PFX_OP)) {
		u_user_num(u, ERR_CHANOPRIVSNEEDED, tc);
		return 1;
	}

	if (c->forward)
		free(c->forward);
	c->forward = strdup(arg);

	u_mode_put(m, on, m->info->ch, " %C", tc);
	return 1;
}

static int cb_key(u_modes *m, int on, char *arg)
{
	u_chan *c = m->target;

	if (cm_deny(m))
		return 1;

	if (!on) {
		if (c->key) {
			free(c->key);
			c->key = NULL;
			u_mode_put(m, on, m->info->ch, " %s", "*");
		}
		return 1;
	}

	if (!arg)
		return 1;

	if (c->key)
		free(c->key);
	c->key = strdup(arg);

	u_mode_put(m, on, m->info->ch, " %s", arg);

	return 1;
}

static int cb_limit(u_modes *m, int on, char *arg)
{
	u_chan *c = m->target;
	int lim;

	if (cm_deny(m))
		return on;

	if (!on) {
		if (c->limit > 0)
			u_mode_put(m, 0, 'l', NULL, NULL);
		c->limit = -1;
		return 0;
	}

	if (arg == NULL)
		return 0;
	lim = atoi(arg);
	if (lim < 1)
		return 1;
	c->limit = lim;

	u_mode_put(m, 1, 'l', " %d", (void*)c->limit);
	return 1;
}

u_chan *u_chan_get(char *name)
{
	return u_trie_get(all_chans, name);
}

u_chan *u_chan_get_or_create(char *name)
{
	u_chan *chan;

	chan = u_chan_get(name);
	if (chan != NULL)
		return chan;

	chan = malloc(sizeof(*chan));
	u_strlcpy(chan->name, name, MAXCHANNAME+1);
	chan->ts = NOW.tv_sec;
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
	chan->invites = u_map_new(0);
	chan->forward = NULL;
	chan->key = NULL;
	chan->limit = -1;

	u_trie_set(all_chans, chan->name, chan);

	return chan;
}

static void drop_list(u_list *list)
{
	u_list *n, *tn;

	U_LIST_EACH_SAFE(n, tn, list) {
		free(n->data);
		u_list_del_n(list, n);
	}
}

static void drop_param(char **p)
{
	if (*p != NULL)
		free(*p);
	*p = NULL;
}

void u_chan_drop(u_chan *chan)
{
	/* TODO: u_map_free callback! */
	u_map_free(chan->members);
	drop_list(&chan->ban);
	drop_list(&chan->quiet);
	drop_list(&chan->banex);
	drop_list(&chan->invex);
	u_clr_invites_chan(chan);
	drop_param(&chan->forward);
	drop_param(&chan->key);

	u_trie_del(all_chans, chan->name);
	free(chan);
}

char *u_chan_modes(u_chan *c, int on_chan)
{
	static char buf[512];
	char chs[64], args[512];
	u_mode_info *info;
	char *s = chs, *p = args;

	*s++ = '+';
	for (info=cmodes; info->ch; info++) {
		if (info->cb == cb_flag && (c->mode & info->data))
			*s++ = info->ch;
	}

	if (c->forward) {
		*s++ = 'f';
		p += sprintf(p, " %s", c->forward);
	}
	if (c->key) {
		*s++ = 'k';
		if (on_chan)
			p += sprintf(p, " %s", c->key);
	}
	if (c->limit >= 0) {
		*s++ = 'l';
		p += sprintf(p, " %d", c->limit);
	}

	*s = *p = '\0';

	sprintf(buf, "%s%s", chs, args);
	return buf;
}

int u_chan_send_topic(u_chan *c, u_user *u)
{
	if (c->topic[0]) {
		u_user_num(u, RPL_TOPIC, c, c->topic);
		u_user_num(u, RPL_TOPICWHOTIME, c, c->topic_setter,
		           c->topic_time);
	} else {
		u_user_num(u, RPL_NOTOPIC, c);
	}

	return 0;
}

struct send_names_priv {
	u_chan *c;
	u_user *u;
	char pfx, *s, buf[512];
	uint w;
};

void send_names_cb(u_map *map, u_user *u, u_chanuser *cu,
                   struct send_names_priv *priv)
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
int u_chan_send_names(u_chan *c, u_user *u)
{
	struct send_names_priv priv;

	priv.c = c;
	priv.u = u;
	priv.pfx = (c->mode&CMODE_PRIVATE)?'*':((c->mode&CMODE_SECRET)?'@':'=');
	priv.s = priv.buf;
	priv.w = 512 - (strlen(me.name) + strlen(u->nick) + strlen(c->name) + 11);

	u_map_each(c->members, (u_map_cb_t*)send_names_cb, &priv);
	if (priv.s != priv.buf)
		u_user_num(u, RPL_NAMREPLY, priv.pfx, c, priv.buf);
	u_user_num(u, RPL_ENDOFNAMES, c);

	return 0;
}

int u_chan_send_list(u_chan *c, u_user *u, u_list *list)
{
	u_list *n;
	u_chanban *ban;
	int entry, end;

	if (list == &c->quiet) {
		entry = RPL_QUIETLIST;
		end = RPL_ENDOFQUIETLIST;
	} else if (list == &c->invex) {
		entry = RPL_INVITELIST;
		end = RPL_ENDOFINVITELIST;
	} else if (list == &c->banex) {
		entry = RPL_EXCEPTLIST;
		end = RPL_ENDOFEXCEPTLIST;
	} else {
		/* shrug */
		entry = RPL_BANLIST;
		end = RPL_ENDOFBANLIST;
	}

	U_LIST_EACH(n, list) {
		ban = n->data;
		u_user_num(u, entry, c, ban->mask, ban->setter, ban->time);
	}
	u_user_num(u, end, c);

	return 0;
}

void u_add_invite(u_chan *c, u_user *u)
{
	/* TODO: check invite limits */
	u_map_set(c->invites, u, u);
	u_map_set(u->invites, c, c);
}

void u_del_invite(u_chan *c, u_user *u)
{
	u_map_del(c->invites, u);
	u_map_del(u->invites, c);
}

int u_has_invite(u_chan *c, u_user *u)
{
	return !!u_map_get(c->invites, u);
}

static void inv_chan_cb(u_map *map, u_user *u, u_user *u_, u_chan *c)
{
	u_del_invite(c, u);
}
void u_clr_invites_chan(u_chan *c)
{
	u_map_each(c->invites, (u_map_cb_t*)inv_chan_cb, c);
}

static void inv_user_cb(u_map *map, u_chan *c, u_chan *c_, u_user *u)
{
	u_del_invite(c, u);
}
void u_clr_invites_user(u_user *u)
{
	u_map_each(u->invites, (u_map_cb_t*)inv_user_cb, u);
}

/* XXX: assumes the chanuser doesn't already exist */
u_chanuser *u_chan_user_add(u_chan *c, u_user *u)
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

void u_chan_user_del(u_chanuser *cu)
{
	u_map_del(cu->c->members, cu->u);
	u_map_del(cu->u->channels, cu->c);
	free(cu);
}

u_chanuser *u_chan_user_find(u_chan *c, u_user *u)
{
	return u_map_get(c->members, u);
}

typedef struct extban extb_t;

struct extban {
	char ch;
	/* struct extban*, u_chan*, u_user*, char *data */
	int (*cb)();
	void *priv;
};

static int ex_oper(extb_t *ex, u_chan *c, u_user *u, char *data)
{
	return u->flags & UMODE_OPER;
}

static int ex_account(extb_t *ex, u_chan *c, u_user *u, char *data)
{
	/* TODO: do this, once we have accounts */
	return 0;
}

static int ex_channel(extb_t *ex, u_chan *c, u_user *u, char *data)
{
	u_chan *tc;
	if (data == NULL)
		return 0;
	tc = u_chan_get(data);
	if (tc == NULL || u_chan_user_find(tc, u) == NULL)
		return 0;
	return 1;
}

static int ex_gecos(extb_t *ex, u_chan *c, u_user *u, char *data)
{
	if (data == NULL)
		return 0;
	return match(data, u->gecos);
}

static extb_t extbans[] = {
	{ 'o', ex_oper, NULL },
	{ 'a', ex_account, NULL },
	{ 'c', ex_channel, NULL },
	{ 'r', ex_gecos, NULL },
	{ 0 }
};

static int matches_ban(u_chan *c, u_user *u, char *mask, char *host)
{
	char *data;
	extb_t *extb = extbans;
	int invert, banned;

	if (*mask == '$') {
		data = strchr(mask, ':');
		if (data != NULL)
			data++;
		mask++;

		invert = 0;
		if (*mask == '~') {
			invert = 1;
			mask++;	
		}

		for (; extb->ch; extb++) {
			if (extb->ch == *mask)
				break;
		}

		if (!extb->ch)
			return 0;

		banned = extb->cb(extb, c, u, data);
		if (invert)
			banned = !banned;
		return banned;
	}

	return match(mask, host);
}

static int is_in_list(u_chan *c, u_user *u, char *host, u_list *list)
{
	u_list *n;
	u_chanban *ban;

	U_LIST_EACH(n, list) {
		ban = n->data;
		if (matches_ban(c, u, ban->mask, host))
			return 1;
	}

	return 0;
}

int u_entry_blocked(u_chan *c, u_user *u, char *key)
{
	char host[BUFSIZE];
	int invited = u_has_invite(c, u);

	snf(FMT_USER, host, BUFSIZE, "%H", u);

	if ((c->mode & CMODE_INVITEONLY)) {
		if (!is_in_list(c, u, host, &c->invex) && !invited)
			return ERR_INVITEONLYCHAN;
	}

	if (c->key != NULL) {
		if (key == NULL || !streq(c->key, key))
			return ERR_BADCHANNELKEY;
	}

	if (is_in_list(c, u, host, &c->ban)) {
		if (!is_in_list(c, u, host, &c->banex))
			return ERR_BANNEDFROMCHAN;
	}

	if (c->limit > 0 && c->members->size >= c->limit && !invited)
		return ERR_CHANNELISFULL;

	/* TODO: an invite also allows +j and +r to be bypassed */

	return 0;
}

u_chan *u_find_forward(u_chan *c, u_user *u, char *key)
{
	int forwards_left = 30;

	while (forwards_left-- > 0) {
		if (c->forward == NULL)
			return NULL;

		c = u_chan_get(c->forward);

		if (c == NULL)
			return NULL;
		if (!u_entry_blocked(c, u, key))
			return c;
	}

	return NULL;
}

int u_is_muted(u_chanuser *cu)
{
	char buf[512];

	snf(FMT_USER, buf, 512, "%H", cu->u);

	if (u_cookie_cmp(&cu->ck_flags, &cu->c->ck_flags) >= 0)
		return cu->flags & CU_MUTED;

	u_cookie_cpy(&cu->ck_flags, &cu->c->ck_flags);
	cu->flags &= ~CU_MUTED;

	if (cu->flags & (CU_PFX_OP | CU_PFX_VOICE))
		return 0;

	if (!is_in_list(cu->c, cu->u, buf, &cu->c->quiet)
	    && !(cu->c->mode & CMODE_MODERATED))
		return 0;

	cu->flags |= CU_MUTED;
	return CU_MUTED; /* not 1, to mimic cu->flags & CU_MUTED */
}

int init_chan(void)
{
	all_chans = u_trie_new(ascii_canonize);
	return all_chans != NULL ? 0 : -1;
}

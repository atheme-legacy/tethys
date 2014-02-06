/* Tethys, chan.c -- channels
   Copyright (c) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

mowgli_patricia_t *all_chans;

static ulong cmode_get_flag_bits(u_modes *m)
{
	return ((u_chan*) m->target)->mode;
}

static bool cmode_set_flag_bits(u_modes *m, ulong fl)
{
	((u_chan*) m->target)->mode |= fl;
	return true;
}

static bool cmode_reset_flag_bits(u_modes *m, ulong fl)
{
	((u_chan*) m->target)->mode &= ~fl;
	return true;
}

static void *cmode_get_status_target(u_modes *m, char *user)
{
	u_chan *c = m->target;
	u_user *u;
	u_chanuser *cu;

	if (!(u = u_user_by_ref(m->setter->source, user))) {
		u_src_num(m->setter, ERR_NOSUCHNICK, user);
		return NULL;
	}

	if (!(cu = u_chan_user_find(c, u))) {
		u_src_num(m->setter, ERR_USERNOTINCHANNEL, c, u);
		return NULL;
	}

	return cu;
}

static bool cmode_set_status_bits(u_modes *m, void *tgt, ulong st)
{
	((u_chanuser*) tgt)->flags |= st;
	return true;
}

static bool cmode_reset_status_bits(u_modes *m, void *tgt, ulong st)
{
	((u_chanuser*) tgt)->flags &= ~st;
	return true;
}

static mowgli_list_t *cmode_get_list(u_modes *m, u_mode_info *info)
{
	u_chan *c = m->target;

	switch (info->ch) {
	case 'b': return &c->ban;
	case 'e': return &c->banex;
	case 'I': return &c->invex;
	case 'q': return &c->quiet;
	}

	return NULL;
}

static void cmode_sync(u_modes *m)
{
	u_chan *c = m->target;
	u_cookie_inc(&c->ck_flags);
}

static int cb_fwd(u_modes*, int, char*);
static int cb_key(u_modes*, int, char*);
static int cb_limit(u_modes*, int, char*);

u_mode_info cmode_infotab[128] = {
	['c'] = { 'c', MODE_FLAG, 0, { .data = CMODE_NOCOLOR } },
	['g'] = { 'g', MODE_FLAG, 0, { .data = CMODE_FREEINVITE } },
	['i'] = { 'i', MODE_FLAG, 0, { .data = CMODE_INVITEONLY } },
	['m'] = { 'm', MODE_FLAG, 0, { .data = CMODE_MODERATED } },
	['n'] = { 'n', MODE_FLAG, 0, { .data = CMODE_NOEXTERNAL } },
	['p'] = { 'p', MODE_FLAG, 0, { .data = CMODE_PRIVATE } },
	['s'] = { 's', MODE_FLAG, 0, { .data = CMODE_SECRET } },
	['t'] = { 't', MODE_FLAG, 0, { .data = CMODE_TOPIC } },
	['z'] = { 'z', MODE_FLAG, 0, { .data = CMODE_OPMOD } },

	['f'] = { 'f', MODE_EXTERNAL, 0, { .fn = cb_fwd } },
	['k'] = { 'k', MODE_EXTERNAL, 0, { .fn = cb_key } },
	['l'] = { 'l', MODE_EXTERNAL, 0, { .fn = cb_limit } },

	['b'] = { 'b', MODE_LIST },
	['q'] = { 'q', MODE_LIST },
	['e'] = { 'e', MODE_LIST },
	['I'] = { 'I', MODE_LIST },

	['o'] = { 'o', MODE_STATUS, 0, { .data = CU_PFX_OP } },
	['v'] = { 'v', MODE_STATUS, 0, { .data = CU_PFX_VOICE } },
};

u_mode_ctx cmodes = {
	.infotab             = cmode_infotab,

	.get_flag_bits       = cmode_get_flag_bits,
	.set_flag_bits       = cmode_set_flag_bits,
	.reset_flag_bits     = cmode_reset_flag_bits,

	.get_status_target   = cmode_get_status_target,
	.set_status_bits     = cmode_set_status_bits,
	.reset_status_bits   = cmode_reset_status_bits,

	.get_list            = cmode_get_list,

	.sync                = cmode_sync,
};

uint cmode_default = CMODE_TOPIC | CMODE_NOEXTERNAL;

/* I can always go back and look at master for what used to be here */

static int cb_fwd(u_modes *m, int on, char *arg)
{
	return 1;
}

static int cb_key(u_modes *m, int on, char *arg)
{
	return 1;
}

static int cb_limit(u_modes *m, int on, char *arg)
{
	return 1;
}

static u_chan *chan_create_real(char *name)
{
	u_chan *chan;

	if (!strchr(CHANTYPES, name[0]))
		return NULL;

	chan = malloc(sizeof(*chan));
	u_strlcpy(chan->name, name, MAXCHANNAME+1);
	chan->ts = NOW.tv_sec;
	chan->topic[0] = '\0';
	chan->topic_setter[0] = '\0';
	chan->topic_time = 0;
	chan->mode = cmode_default;
	chan->flags = 0;
	u_cookie_reset(&chan->ck_flags);
	chan->members = u_map_new(0);
	mowgli_list_init(&chan->ban);
	mowgli_list_init(&chan->quiet);
	mowgli_list_init(&chan->banex);
	mowgli_list_init(&chan->invex);
	chan->invites = u_map_new(0);
	chan->forward = NULL;
	chan->key = NULL;
	chan->limit = -1;

	if (name[0] == '&')
		chan->flags |= CHAN_LOCAL;

	mowgli_patricia_add(all_chans, chan->name, chan);

	return chan;
}

u_chan *u_chan_get(char *name)
{
	return mowgli_patricia_retrieve(all_chans, name);
}

u_chan *u_chan_create(char *name)
{
	if (u_chan_get(name))
		return NULL;

	return chan_create_real(name);
}

u_chan *u_chan_get_or_create(char *name)
{
	u_chan *chan;

	chan = u_chan_get(name);
	if (chan != NULL)
		return chan;

	if (name[0] != '#') /* TODO: hnggg!!! */
		return NULL;

	return chan_create_real(name);
}

static void drop_list(mowgli_list_t *list)
{
	mowgli_node_t *n, *tn;

	MOWGLI_LIST_FOREACH_SAFE(n, tn, list->head) {
		mowgli_node_delete(n, list);
		free(n->data);
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
	/* TODO: send PART to all users in this channel! */
	u_map_free(chan->members);
	drop_list(&chan->ban);
	drop_list(&chan->quiet);
	drop_list(&chan->banex);
	drop_list(&chan->invex);
	u_clr_invites_chan(chan);
	drop_param(&chan->forward);
	drop_param(&chan->key);

	mowgli_patricia_delete(all_chans, chan->name);
	free(chan);
}

char *u_chan_modes(u_chan *c, int on_chan)
{
	static char buf[512];
	char chs[64], args[512];
	const char *bit = CMODE_BITS;
	char *s = chs, *p = args;
	ulong mode = c->mode;

	*s++ = '+';
	for (; mode; bit++, mode >>= 1) {
		if (mode & 1)
			*s++ = *bit;
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

/* :my.name 353 nick = #chan :...
   *       *****    ***     **  = 11 */
int u_chan_send_names(u_chan *c, u_user *u)
{
	u_map_each_state st;
	u_strop_wrap wrap;
	u_user *tu;
	u_chanuser *cu;
	char *s, pfx;
	int sz;

	pfx = c->mode & CMODE_PRIVATE ? '*'
	    : c->mode & CMODE_SECRET ? '@'
	    : '=';

	sz = strlen(me.name) + strlen(u->nick) + strlen(c->name) + 11;
	u_strop_wrap_start(&wrap, 510 - sz);
	U_MAP_EACH(&st, c->members, &tu, &cu) {
		char *p, nbuf[MAXNICKLEN+3];

		p = nbuf;
		if (cu->flags & CU_PFX_OP)
			*p++ = '@';
		if ((cu->flags & CU_PFX_VOICE)
		    && (p == nbuf || u->flags & CAP_MULTI_PREFIX))
			*p++ = '+';
		strcpy(p, tu->nick);

		while ((s = u_strop_wrap_word(&wrap, nbuf)) != NULL)
			u_user_num(u, RPL_NAMREPLY, pfx, c, s);
	}
	if ((s = u_strop_wrap_word(&wrap, NULL)) != NULL)
		u_user_num(u, RPL_NAMREPLY, pfx, c, s);

	u_user_num(u, RPL_ENDOFNAMES, c);

	return 0;
}

int u_chan_send_list(u_chan *c, u_user *u, mowgli_list_t *list)
{
	mowgli_node_t *n;
	u_listent *ban;
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

	MOWGLI_LIST_FOREACH(n, list->head) {
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
	u_chan *c = cu->c;
	u_user *u = cu->u;

	u_map_del(c->members, u);
	u_map_del(u->channels, c);

	free(cu);

	if (c->members->size == 0) {
		u_log(LG_DEBUG, "u_chan_user_del: %C empty, dropping...", c);
		u_chan_drop(c);
	}
}

u_chanuser *u_chan_user_find(u_chan *c, u_user *u)
{
	return u_map_get(c->members, u);
}

typedef struct extban extb_t;

struct extban {
	char ch;
	int (*cb)(struct extban*, u_chan*, u_user*, char *data);
	void *priv;
};

static int ex_oper(extb_t *ex, u_chan *c, u_user *u, char *data)
{
	return u->mode & UMODE_OPER;
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

static int is_in_list(u_chan *c, u_user *u, char *host, mowgli_list_t *list)
{
	mowgli_node_t *n;
	u_listent *ban;

	MOWGLI_LIST_FOREACH(n, list->head) {
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
	if (!(all_chans = mowgli_patricia_create(ascii_canonize)))
		return -1;

	return 0;
}

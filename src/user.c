/* ircd-micro, user.c -- user management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_umode_info __umodes[32] = {
	{ 'o', UMODE_OPER },
	{ 'i', UMODE_INVISIBLE },
	{ 'w', UMODE_WALLOPS },
	{ 'x', UMODE_CLOAKED },
	{ 0, 0 }
};

u_umode_info *umodes = __umodes;
uint umode_default = UMODE_INVISIBLE;

u_trie *users_by_nick;
u_trie *users_by_uid;

char *id_map = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
int id_modulus = 36; /* just strlen(uid_map) */
int id_digits[6] = {0, 0, 0, 0, 0, 0};
char id_buf[7] = {0, 0, 0, 0, 0, 0, 0};

char *id_next()
{
	int i;
	for (i=0; i<6; i++)
		id_buf[i] = id_map[id_digits[i]];
	for (i=6; i-->0;) {
		id_digits[i] ++;
		if (id_digits[i] < id_modulus)
			break;
		id_digits[i] -= id_modulus;
	}
	id_buf[6] = '\0';
	return id_buf;
}

/* used to simplify user_local_event */
void user_local_die(conn, msg) u_conn *conn; char *msg;
{
	u_user *u = conn->priv;
	u_user_unlink(u, msg);
}

void user_local_event(conn, event) u_conn *conn;
{
	switch (event) {
	case EV_ERROR:
		user_local_die(conn, conn->error);
		break;
	default:
		user_local_die(conn, "unknown error");
		break;

	case EV_DESTROYING:
		free(conn->priv);
	}
}

void u_user_make_ureg(conn) u_conn *conn;
{
	u_user_local *ul;
	u_user *u;

	if (conn->ctx != CTX_UNREG && conn->ctx != CTX_UREG)
		return;

	conn->ctx = CTX_UREG;

	if (conn->priv != NULL)
		return;

	conn->priv = ul = malloc(sizeof(*ul));
	memset(ul, 0, sizeof(*ul));

	u = USER(ul);

	u_strlcpy(u->uid, me.sid, 4);
	u_strlcpy(u->uid + 3, id_next(), 7);
	u_trie_set(users_by_uid, u->uid, u);

	u_strlcpy(u->host, conn->host, MAXHOST+1);

	u->flags = umode_default | USER_IS_LOCAL;
	u_user_state(u, USER_REGISTERING);
	u->channels = u_map_new(0);

	ul->conn = conn;

	conn->event = user_local_event;

	u_log(LG_DEBUG, "New user uid=%s host=%s", u->uid, u->host);
}

void user_quit_cb(map, c, cu, priv)
u_map *map; u_chan *c; u_chanuser *cu; void *priv;
{
	u_chan_user_del(cu);
}

void u_user_unlink(u, msg) u_user *u; char *msg;
{
	u_conn *conn = u_user_conn(u);

	if (u_user_state(u, 0) == USER_DISCONNECTED)
		return;

	u_user_state(u, USER_DISCONNECTED);

	if (conn->ctx == CTX_USER) {
		u_sendto_visible(u, ":%H QUIT :%s", u, msg);
		u_conn_f(conn, ":%H QUIT :%s", u, msg);
	} else {
		u_conn_f(conn, "ERROR :%s", msg);
	}

	/* part from all channels */
	u_map_each(u->channels, user_quit_cb, NULL);
	u_map_free(u->channels);

	if (u->nick[0])
		u_trie_del(users_by_nick, u->nick);
	u_trie_del(users_by_uid, u->uid);
}

u_conn *u_user_conn(u) u_user *u;
{
	if (u->flags & USER_IS_LOCAL) {
		return ((u_user_local*)u)->conn;
	} else {
		return ((u_user_remote*)u)->server->conn;
	}
}

u_user *u_user_by_nick(nick) char *nick;
{
	return u_trie_get(users_by_nick, nick);
}

u_user *u_user_by_uid(uid) char *uid;
{
	return u_trie_get(users_by_uid, uid);
}

void u_user_set_nick(u, nick) u_user *u; char *nick;
{
	if (u->nick[0])
		u_trie_del(users_by_nick, u->nick);
	u_strlcpy(u->nick, nick, MAXNICKLEN+1);
	u_trie_set(users_by_nick, u->nick, u);
}

uint u_user_state(u, state) u_user *u; uint state;
{
	if (state & USER_MASK_STATE) {
		u->flags &= ~USER_MASK_STATE;
		u->flags |= (state & USER_MASK_STATE);
	}

	return u->flags & USER_MASK_STATE;
}

void u_user_vnum(u, num, va) u_user *u; va_list va;
{
	u_conn *conn;
	char *nick = u->nick;
	if (!*nick)
		nick = "*";

	if (u->flags & USER_IS_LOCAL) {
		conn = ((u_user_local*)u)->conn;
	} else {
		u_log(LG_SEVERE, "Can't send numerics to remote users yet!");
		return;
	}

	u_conn_vnum(conn, nick, num, va);
}

#ifdef STDARG
void u_user_num(u_user *u, int num, ...)
#else
void u_user_num(u, num, va_alist) u_user *u; va_dcl
#endif
{
	va_list va; 
	u_va_start(va, num);
	u_user_vnum(u, num, va);
	va_end(va);
}

void u_user_welcome(ul) u_user_local *ul;
{
	u_user *u = USER(ul);

	u_log(LG_DEBUG, "user: welcoming %s", u->nick);

	u_user_state(u, USER_CONNECTED);
	ul->conn->ctx = CTX_USER;

	u_user_num(u, RPL_WELCOME, my_net_name, u->nick);
	u_user_num(u, RPL_YOURHOST, me.name, PACKAGE_FULLNAME);
	u_user_num(u, RPL_CREATED, date);
	u_user_send_motd((u_user_local*)u);
}

void u_user_send_motd(ul) u_user_local *ul;
{
	u_list *n;
	u_user *u = USER(ul);

	if (u_list_is_empty(&my_motd)) {
		u_user_num(u, ERR_NOMOTD);
		return;
	}

	u_user_num(u, RPL_MOTDSTART, me.name);
	U_LIST_EACH(n, &my_motd)
		u_user_num(u, RPL_MOTD, n->data);
	u_user_num(u, RPL_ENDOFMOTD);
}

static int entry_blocked(c, u, key) u_chan *c; u_user *u; char *key;
{
	char hostmask[BUFSIZE];
	u_list *n;
	char *ban;

	if (c->mode & CMODE_INVITEONLY) {
		/* TODO: check pending invites */
		return ERR_INVITEONLYCHAN;
	}

	if (c->key != NULL) {
		if (key == NULL || strcmp(c->key, key) != 0)
			return ERR_BADCHANNELKEY;
	}

	snf(FMT_USER, hostmask, BUFSIZE, "%H", u);
	U_LIST_EACH(n, &c->ban) {
		ban = n->data;
		if (match(ban, hostmask))
			return ERR_BANNEDFROMCHAN;
	}

	/* TODO: check +l */

	return 0;
}

static void do_join_chan(c, u) u_chan *c; u_user *u;
{
	u_conn *conn = u_user_conn(u);
	u_chanuser *cu;

	cu = u_chan_user_add(c, u);

	if (c->members->size == 1) {
		u_log(LG_DEBUG, "Channel %C created", c);
		cu->flags |= CU_PFX_OP;
	}

	u_sendto_chan(c, NULL, ":%H JOIN %C", u, c);
	if (c->members->size == 1) /* idk why charybdis does it this way */
		u_conn_f(conn, ":%S MODE %C %s", &me, c, u_chan_modes(c));
	u_chan_send_topic(c, u);
	u_chan_send_names(c, u);
}

static u_chan *find_forward(c, u, key) u_chan *c; u_user *u; char *key;
{
	/* TODO: determine how to detect forward loops */

	int forwards_left = 30;

	while (forwards_left-- > 0) {
		if (c->forward == NULL)
			return NULL;

		c = u_chan_get(c->forward);

		if (c == NULL)
			return NULL;
		if (!entry_blocked(c, u, key))
			return c;
	}

	return NULL;
}

void u_user_try_join_chan(ul, chan, key) u_user_local *ul; char *chan, *key;
{
	u_user *u = USER(ul);
	u_chan *c, *fwd;
	u_chanuser *cu;
	int num;

	c = u_chan_get_or_create(chan);
	
	if (c == NULL) {
		u_user_num(u, ERR_GENERIC, "Can't get or create channel!");
		return;
	}

	cu = u_chan_user_find(c, u);
	if (cu != NULL)
		return;

	num = entry_blocked(c, u, key);
	if (num != 0) {
		fwd = find_forward(c, u, key);
		if (fwd == NULL) {
			u_user_num(u, num, c);
			return;
		}
		c = fwd;
	}

	do_join_chan(c, u);
}

void u_user_part_chan(ul, chan, reason) u_user_local *ul; char *chan, *reason;
{
	char buf[512];
	u_user *u = USER(ul);
	u_chan *c;
	u_chanuser *cu;

	c = u_chan_get(chan);
	if (c == NULL) {
		u_user_num(u, ERR_NOSUCHCHANNEL, chan);
		return;
	}

	cu = u_chan_user_find(c, u);
	if (cu == NULL) {
		u_user_num(u, ERR_NOTONCHANNEL, c);
		return;
	}

	buf[0] = '\0';
	if (reason)
		sprintf(buf, " :%s", reason);

	u_sendto_chan(c, NULL, ":%H PART %C%s", u, c, buf);

	u_chan_user_del(cu);

	if (c->members->size == 0) {
		u_log(LG_DEBUG, "Dropping channel %C", c);
		u_chan_drop(c);
	}
}

int init_user()
{
	users_by_nick = u_trie_new(rfc1459_canonize);
	users_by_uid = u_trie_new(ascii_canonize);

	return (users_by_nick && users_by_uid) ? 0 : -1;
}

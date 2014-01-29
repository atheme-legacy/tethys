/* Tethys, user.c -- user management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

mowgli_patricia_t *users_by_nick;
mowgli_patricia_t *users_by_uid;

char *id_map = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
int id_modulus = 36; /* just strlen(uid_map) */
int id_digits[6] = {0, 0, 0, 0, 0, 0};
char id_buf[7] = {0, 0, 0, 0, 0, 0, 0};

char *id_next(void)
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

static void cb_oper();
static void cb_flag();

static u_umode_info __umodes[32] = {
	{ 'o', UMODE_OPER,      cb_oper         },
	{ 'i', UMODE_INVISIBLE, cb_flag         },
	{ 0 }
};

u_umode_info *umodes = __umodes;
uint umode_default = 0;

static int um_on;
static char *um_buf_p, um_buf[128];

void u_user_m_start(u_user *u)
{
	um_on = -1;
	um_buf_p = um_buf;
}

void u_user_m_end(u_user *u)
{
	*um_buf_p = '\0';
	if (um_buf_p != um_buf) {
		u_conn_f(u_user_conn(u), ":%U MODE %U :%s", u, u, um_buf);
		u_sendto_servers(NULL, ":%U MODE %U :%s", u, u, um_buf);
	} else if (um_on < 0) {
		u_user_num(u, ERR_UMODEUNKNOWNFLAG);
	}
}

static void um_put(int on, char ch)
{
	if (on != um_on) {
		um_on = on;
		*um_buf_p++ = on ? '+' : '-';
	}
	*um_buf_p++ = ch;
}

static void cb_oper(u_umode_info *info, u_user *u, int on)
{
	if (on)
		return;

	u->flags &= ~info->mask;
	um_put(on, info->ch);
}

static void cb_flag(u_umode_info *info, u_user *u, int on)
{
	uint oldm = u->flags;
	if (on)
		u->flags |= info->mask;
	else
		u->flags &= ~info->mask;
	if (oldm != u->flags)
		um_put(on, info->ch);
}

void u_user_mode(u_user *u, char ch, int on)
{
	u_umode_info *info = umodes;

	while (info->ch && info->ch != ch)
		info++;

	if (!info->ch)
		return;

	info->cb(info, u, on);
}

void user_shutdown(u_conn *conn)
{
	char *msg = conn->error ? conn->error : "conn shutdown";
	u_user *u = conn->priv;
	if (u == NULL)
		return;
	if (conn->ctx == CTX_USER)
		u_sendto_visible(u, ST_ALL, ":%H QUIT :%s", u, msg);
	u_conn_f(conn, "ERROR :%s", msg);
	u_user_destroy(u);
}

static u_user *create_user(char *uid, size_t sz)
{
	u_user *u;

	if (!(u = calloc(1, sz))) {
		u_log(LG_SEVERE, "calloc() failed");
		abort();
	}

	u_strlcpy(u->uid, uid, 10);
	mowgli_patricia_add(users_by_uid, u->uid, u);

	u->channels = u_map_new(0);
	u->invites = u_map_new(0);

	return u;
}

u_user *u_user_create_local(u_conn *conn)
{
	char uid[10];
	u_user *u;

	if (!conn || conn->priv || conn->ctx != CTX_NONE)
		return NULL;

	snprintf(uid, 10, "%s%s", me.sid, id_next());
	u = create_user(uid, sizeof(u_user_local));

	u->flags = umode_default | USER_IS_LOCAL;

	USER_LOCAL(u)->conn = conn;
	conn->ctx = CTX_USER;
	conn->priv = u;
	conn->shutdown = user_shutdown;

	me.nusers++;
	u_log(LG_VERBOSE, "New local user, uid=%s", u->uid);

	return u;
}

u_user *u_user_create_remote(u_server *sv, char *uid)
{
	u_user * u;

	if (strncmp(uid, sv->sid, 3) != 0) {
		u_log(LG_WARN, "Adding remote user with wrong SID!");
		u_log(LG_INFO, "     uid=%s, sv->sid=%s", uid, sv->sid);
	}
	u = create_user(uid, sizeof(u_user_remote));

	USER_REMOTE(u)->server = sv;
	sv->nusers++;

	u_log(LG_VERBOSE, "New remote user, uid=%s", u->uid);

	return u;
}

void user_destroy_cb(u_map *map, u_chan *c, u_chanuser *cu, void *priv)
{
	u_chan_user_del(cu);
}

void u_user_destroy(u_user *u)
{
	u_server *sv = u_user_server(u);

	u_log(LG_VERBOSE, "Destroying user uid=%s (%U)", u->uid, u);

	if (IS_LOCAL_USER(u)) {
		u_user_local *ul = USER_LOCAL(u);
		if (ul->conn) {
			ul->conn->priv = NULL;
			u_conn_shutdown(ul->conn);
			ul->conn = NULL;
		}
	}

	u_clr_invites_user(u);

	/* part from all channels */
	u_map_each(u->channels, (u_map_cb_t*)user_destroy_cb, NULL);
	u_map_free(u->channels);

	if (u->nick[0])
		mowgli_patricia_delete(users_by_nick, u->nick);
	mowgli_patricia_delete(users_by_uid, u->uid);

	if (sv != NULL)
		sv->nusers--;

	free(u);
}

void u_user_try_register(u_user *u)
{
	u_user_local *ul;
	u_conn *conn;

	if (!IS_LOCAL_USER(u))
		return;
	ul = USER_LOCAL(u);
	conn = ul->conn;
	if (!conn || (conn->flags & U_CONN_REGISTERED))
		return;

	if (!conn->host[0])
		return;

	if (!u->nick[0] || !u->ident[0])
		return;

	if (u->flags & USER_MASK_WAIT)
		return;

	conn->flags |= U_CONN_REGISTERED;
	u_strlcpy(u->ip, conn->ip, INET_ADDRSTRLEN);
	u_strlcpy(u->realhost, conn->host, MAXHOST+1);
	u_strlcpy(u->host, conn->host, MAXHOST+1);
	u_user_welcome(ul);
}

u_conn *u_user_conn(u_user *u)
{
	if (u == NULL)
		return NULL;
	if (IS_LOCAL_USER(u))
		return USER_LOCAL(u)->conn;
	else
		return USER_REMOTE(u)->server->conn;
}

u_server *u_user_server(u_user *u)
{
	if (u == NULL)
		return NULL;
	if (IS_LOCAL_USER(u))
		return &me;
	else
		return USER_REMOTE(u)->server;
}

u_user *u_user_by_nick(char *nick)
{
	return mowgli_patricia_retrieve(users_by_nick, nick);
}

u_user *u_user_by_uid(char *uid)
{
	return mowgli_patricia_retrieve(users_by_uid, uid);
}

void u_user_set_nick(u_user *u, char *nick, uint ts)
{
	/* TODO: check collision? */
	if (u->nick[0])
		mowgli_patricia_delete(users_by_nick, u->nick);
	u_strlcpy(u->nick, nick, MAXNICKLEN+1);
	mowgli_patricia_add(users_by_nick, u->nick, u);
	u->nickts = ts;
}

void u_user_vnum(u_user *u, int num, va_list va)
{
	u_conn *conn;
	char *tgt;

	if (!IS_LOCAL_USER(u)) {
		tgt = u->uid;
	} else {
		tgt = u->nick;
		if (!*tgt)
			tgt = "*";
	}

	conn = u_user_conn(u);
	u_conn_vnum(conn, tgt, num, va);
}

int u_user_num(u_user *u, int num, ...)
{
	va_list va; 
	if (!u)
		return 0;
	va_start(va, num);
	u_user_vnum(u, num, va);
	va_end(va);
	return 0;
}

struct isupport {
	char *name;
	char *s;
	int i;
} isupport[] = {
	{ "PREFIX",       "(ov)@+"                },
	{ "CHANTYPES",    CHANTYPES               },
	{ "CHANMODES",    "beIq,k,fl,cgimnpstz"   },
	{ "MODES",        NULL, 4                 },
	{ "MAXLIST",      NULL, MAXBANLIST        },
	{ "CASEMAPPING",  "rfc1459"               },
	{ "NICKLEN",      NULL, MAXNICKLEN        },
	{ "TOPICLEN",     NULL, MAXTOPICLEN       },
	{ "CHANNELLEN",   NULL, MAXCHANNAME       },
	{ "AWAYLEN",      NULL, MAXAWAY           },
	{ "MAXTARGETS",   NULL, 1                 },
	{ "EXCEPTS"                               },
	{ "INVEX"                                 },
	{ "FNC"                                   },
	{ "WHOX" /* TODO: this */                 },
	{ NULL },
};

void u_user_send_isupport(u_user *u)
{
	/* :host.irc 005 nick ... :are supported by this server
	   *        *****    *   *....*....*....*....*....*.... = 37 */
	struct isupport *cur;
	char *s, *p, buf[512], tmp[512];
	int w;

	w = 475 - strlen(me.name) - strlen(u->nick);

	s = buf;
	for (cur=isupport; cur->name; cur++) {
		p = tmp;
		if (cur->s) {
			sprintf(tmp, "%s=%s", cur->name, cur->s);
		} else if (cur->i) {
			sprintf(tmp, "%s=%d", cur->name, cur->i);
		} else {
			p = cur->name;
		}

again:
		if (!wrap(buf, &s, w, p)) {
			u_user_num(u, RPL_ISUPPORT, buf);
			goto again;
		}
	}
	if (s != buf)
		u_user_num(u, RPL_ISUPPORT, buf);
}

void u_user_send_motd(u_user *u)
{
	mowgli_node_t *n;

	if (mowgli_list_size(&my_motd) == 0) {
		u_user_num(u, ERR_NOMOTD);
		return;
	}

	u_user_num(u, RPL_MOTDSTART, me.name);
	MOWGLI_LIST_FOREACH(n, my_motd.head)
		u_user_num(u, RPL_MOTD, n->data);
	u_user_num(u, RPL_ENDOFMOTD);
}

void u_user_welcome(u_user_local *ul)
{
	u_user *u = USER(ul);
	char buf[512];

	u_log(LG_DEBUG, "user: welcoming %s", u->nick);

	u_user_num(u, RPL_WELCOME, my_net_name, u->nick);
	u_user_num(u, RPL_YOURHOST, me.name, PACKAGE_FULLNAME);
	u_user_num(u, RPL_CREATED, startedstr);
	u_user_send_isupport(u);
	u_user_send_motd(u);

	u_user_make_euid(u, buf);
	u_sendto_servers(NULL, "%s", buf);
}

static int is_in_list(char *host, mowgli_list_t *list)
{
	mowgli_node_t *n;
	u_chanban *ban;

	if (!host || !list)
		return 0;

	MOWGLI_LIST_FOREACH(n, list->head) {
		ban = n->data;
		if (match(ban->mask, host))
			return 1;
	}

	return 0;
}

int u_user_in_list(u_user *u, mowgli_list_t *list)
{
	char buf[512];
	snf(FMT_USER, buf, 512, "%H", u);
	return is_in_list(buf, list);
}

void u_user_make_euid(u_user *u, char *buf)
{
	u_server *sv = u_user_server(u);

	/* still ridiculous...              nick  nickts   host  uid   acct
                                               hops  modes    ip    rlhost gecos
                                                        ident                    */
	snf(FMT_SERVER, buf, 512, ":%S EUID %s %d %u %s %s %s %s %s %s %s :%s",
	    sv, u->nick, sv->hops + 1, u->nickts,
	    "+", /* TODO: this */
	    u->ident, u->host, u->ip, u->uid, u->realhost,
	    u->acct[0] ? u->acct : "*", u->gecos);
}

int init_user(void)
{
	users_by_nick = mowgli_patricia_create(rfc1459_canonize);
	users_by_uid = mowgli_patricia_create(ascii_canonize);

	if (!users_by_nick || !users_by_uid)
		return -1;

	return 0;
}

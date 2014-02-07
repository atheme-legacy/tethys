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

static ulong umode_get_flag_bits(u_modes *m)
{
	return ((u_user*) m->target)->mode;
}

static bool umode_set_flag_bits(u_modes *m, ulong fl)
{
	((u_user*) m->target)->mode |= fl;
	return true;
}

static bool umode_reset_flag_bits(u_modes *m, ulong fl)
{
	((u_user*) m->target)->mode &= ~fl;
	return true;
}

u_mode_info umode_infotab[128] = {
	['o'] = { 'o', MODE_FLAG, MODE_NO_SET,    { .data = UMODE_OPER } },
	['i'] = { 'i', MODE_FLAG, 0,              { .data = UMODE_INVISIBLE } },
	['S'] = { 'S', MODE_FLAG, MODE_NO_CHANGE, { .data = UMODE_SERVICE } },
};

u_mode_ctx umodes = {
	.infotab             = umode_infotab,

	.get_flag_bits       = umode_get_flag_bits,
	.set_flag_bits       = umode_set_flag_bits,
	.reset_flag_bits     = umode_reset_flag_bits,
};

uint umode_default = 0;

void user_shutdown(u_conn *conn)
{
	char *msg = conn->error ? conn->error : "conn shutdown";
	u_user *u = conn->priv;
	if (u == NULL)
		return;
	if (conn->ctx == CTX_USER) {
		u_sendto_visible(u, ST_USERS, ":%H QUIT :%s", u, msg);
		u_sendto_servers(NULL, ":%H QUIT :%s", u, msg);
	}
	u_conn_f(conn, "ERROR :%s", msg);
	u_user_destroy(u);
}

static u_user *create_user(char *uid, u_conn *link, u_server *sv)
{
	u_user *u;

	if (!(u = calloc(1, sizeof(*u)))) {
		u_log(LG_SEVERE, "calloc() failed");
		abort();
	}

	u_strlcpy(u->uid, uid, 10);
	mowgli_patricia_add(users_by_uid, u->uid, u);

	u->channels = u_map_new(0);
	u->invites = u_map_new(0);

	u_ratelimit_init(u);

	u->link = link;
	u->oper = NULL;
	u->sv = sv;

	u->sv->nusers++;

	return u;
}

u_user *u_user_create_local(u_conn *conn)
{
	char uid[10];
	u_user *u;

	if (!conn || conn->priv || conn->ctx != CTX_NONE)
		return NULL;

	snprintf(uid, 10, "%s%s", me.sid, id_next());
	u = create_user(uid, conn, &me);

	u->mode = umode_default;
	u->flags = USER_IS_LOCAL;

	conn->ctx = CTX_USER;
	conn->priv = u;
	conn->shutdown = user_shutdown;

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
	u = create_user(uid, sv->conn, &me);

	u_log(LG_VERBOSE, "New remote user, uid=%s", u->uid);

	return u;
}

void user_destroy_cb(u_map *map, u_chan *c, u_chanuser *cu, void *priv)
{
	u_chan_user_del(cu);
}

void u_user_destroy(u_user *u)
{
	u_log(LG_VERBOSE, "Destroying user uid=%s (%U)", u->uid, u);

	if (IS_LOCAL_USER(u)) {
		if (u->link) {
			u->link->priv = NULL;
			u_conn_shutdown(u->link);
			u->link = NULL;
		}
	}

	u_clr_invites_user(u);

	/* part from all channels */
	u_map_each(u->channels, (u_map_cb_t*)user_destroy_cb, NULL);
	u_map_free(u->channels);

	if (u->nick[0])
		mowgli_patricia_delete(users_by_nick, u->nick);
	mowgli_patricia_delete(users_by_uid, u->uid);

	u->sv->nusers--;

	free(u);
}

void u_user_try_register(u_user *u)
{
	if (!IS_LOCAL_USER(u))
		return;

	if (!u->link || (u->link->flags & U_CONN_REGISTERED))
		return;

	if (!u->link->host[0])
		return;

	if (!u->nick[0] || !u->ident[0])
		return;

	if (u->flags & USER_MASK_WAIT)
		return;

	if (!(u->link->auth = u_find_auth(u->link))) {
		u_conn_fatal(u->link, "No auth blocks for your host");
		return;
	}

	u->link->flags |= U_CONN_REGISTERED;
	u_strlcpy(u->ip, u->link->ip, INET_ADDRSTRLEN);
	u_strlcpy(u->realhost, u->link->host, MAXHOST+1);
	u_strlcpy(u->host, u->link->host, MAXHOST+1);
	u_user_welcome(u);
}

u_user *u_user_by_nick(char *nick)
{
	return mowgli_patricia_retrieve(users_by_nick, nick);
}

u_user *u_user_by_uid(char *uid)
{
	return mowgli_patricia_retrieve(users_by_uid, uid);
}

char *u_user_modes(u_user *u)
{
	static char buf[512];
	char *s = buf;

	*s++ = '+';
	/*
	for (info=umodes; info->ch; info++) {
		if (info->cb == cb_flag && (u->mode & info->data))
			*s++ = info->ch;
	}
	*/

	if (u->mode & UMODE_INVISIBLE)
		*s++ = 'i';
	if (u->mode & UMODE_OPER)
		*s++ = 'o';
	if (u->mode & UMODE_SERVICE)
		*s++ = 'S';

	*s = '\0';

	return buf;
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
	char *tgt;

	if (!IS_LOCAL_USER(u)) {
		tgt = u->uid;
	} else {
		tgt = u->nick;
		if (!*tgt)
			tgt = "*";
	}

	u_conn_vnum(u->link, tgt, num, va);
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
	u_strop_wrap wrap;
	char *s, *p, tmp[512];

	u_strop_wrap_start(&wrap, 510 - 37 - strlen(me.name) - strlen(u->nick));

	for (cur=isupport; cur->name; cur++) {
		p = tmp;
		if (cur->s) {
			sprintf(tmp, "%s=%s", cur->name, cur->s);
		} else if (cur->i) {
			sprintf(tmp, "%s=%d", cur->name, cur->i);
		} else {
			p = cur->name;
		}

		while ((s = u_strop_wrap_word(&wrap, p)) != NULL)
			u_user_num(u, RPL_ISUPPORT, s);
	}
	if ((s = u_strop_wrap_word(&wrap, NULL)) != NULL)
		u_user_num(u, RPL_ISUPPORT, s);
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

void u_user_welcome(u_user *u)
{
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
	u_listent *ban;

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
	/* still ridiculous...              nick  nickts   host  uid   acct
                                               hops  modes    ip    rlhost gecos
                                                        ident                    */
	snf(FMT_SERVER, buf, 512, ":%S EUID %s %d %u %s %s %s %s %s %s %s :%s",
	    u->sv, u->nick, u->sv->hops + 1, u->nickts,
	    u_user_modes(u),
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

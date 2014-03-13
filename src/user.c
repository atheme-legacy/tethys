/* Tethys, user.c -- user management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

mowgli_patricia_t *users_by_nick;
mowgli_patricia_t *users_by_uid;

char *id_map = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
int id_modulus = 36; /* just strlen(uid_map) */
char id_digits[6] = {0, 0, 0, 0, 0, 0};
char id_buf[7] = {0, 0, 0, 0, 0, 0, 0};

char *id_next(void)
{
	int i;
	for (i=0; i<6; i++)
		id_buf[i] = id_map[(int)id_digits[i]];
	for (i=6; i-->0;) {
		id_digits[i] ++;
		if (id_digits[i] < id_modulus)
			break;
		id_digits[i] -= id_modulus;
	}
	id_buf[6] = '\0';
	return id_buf;
}

static int set_id_next_from_str(const char *id)
{
	char d[6] = {};
	char *x;
	int i;

	for (i=0;i<6;++i) {
		x = strchr(id_map, id[i]);
		if (!x)
			return -1;
		d[i] = x - id_map;
	}

	memcpy(id_digits, d, 6);
	return 0;
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

static u_user *create_user(const char *uid, u_link *link, u_server *sv)
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

u_user *u_user_create_local(u_link *link)
{
	char uid[10];
	u_user *u;

	if (!link || link->priv || link->type != LINK_NONE)
		return NULL;

	snprintf(uid, 10, "%s%s", me.sid, id_next());
	u = create_user(uid, link, &me);

	u->mode = umode_default;
	u->flags = USER_IS_LOCAL;

	link->type = LINK_USER;
	link->priv = u;

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
	u = create_user(uid, sv->link, sv);

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

	if (!u->link || (u->link->flags & U_LINK_REGISTERED))
		return;

	if (!u->link->conn->host[0])
		return;

	if (!u->nick[0] || !u->ident[0])
		return;

	if (u->flags & USER_MASK_WAIT)
		return;

	if (!(u->link->conf.auth = u_find_auth(u->link))) {
		u_link_fatal(u->link, "No auth blocks for your host");
		return;
	}
	u->link->sendq = u->link->conf.auth->cls->sendq;

	u->link->flags |= U_LINK_REGISTERED;
	u_strlcpy(u->ip, u->link->conn->ip, INET_ADDRSTRLEN);
	u_strlcpy(u->realhost, u->link->conn->host, MAXHOST+1);
	u_strlcpy(u->host, u->link->conn->host, MAXHOST+1);
	u_user_welcome(u);
}

u_user *u_user_by_nick_raw(const char *nick)
{
	return mowgli_patricia_retrieve(users_by_nick, nick);
}

u_user *u_user_by_nick(const char *nick)
{
	u_user *u = u_user_by_nick_raw(nick);
	return u && IS_REGISTERED(u) ? u : NULL;
}

u_user *u_user_by_uid_raw(const char *uid)
{
	return mowgli_patricia_retrieve(users_by_uid, uid);
}

u_user *u_user_by_uid(const char *nick)
{
	u_user *u = u_user_by_uid_raw(nick);
	return u && IS_REGISTERED(u) ? u : NULL;
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

bool u_user_try_override(u_user *u)
{
	if (!(IS_LOCAL_USER(u)))
		return false;

	if (u->link->flags & U_LINK_REGISTERED)
		return false;

	u_user_num(u, ERR_NICKNAMEINUSE, u->nick);
	mowgli_patricia_delete(users_by_nick, u->nick);
	u->nick[0] = '\0';

	return true;
}

void u_user_vnum(u_user *u, int num, va_list va)
{
	char *tgt;

	if (!IS_LOCAL_USER(u)) {
		tgt = u->uid;
	} else {
		tgt = u->nick;
		if (!IS_REGISTERED(u))
			tgt = "*";
	}

	u_link_vnum(u->link, tgt, num, va);
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
	{ "PREFIX",                               },
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
	mowgli_node_t *n;

	u_strop_wrap_start(&wrap, 510 - 37 - strlen(me.name) - strlen(u->nick));

	for (cur=isupport; cur->name; cur++) {
		p = tmp;
		if (streq(cur->name, "PREFIX")) {
			s = tmp + sprintf(tmp, "PREFIX=");
			*s++ = '(';
			MOWGLI_LIST_FOREACH(n, cu_pfx_list.head) {
				u_cu_pfx *cs = n->data;
				*s++ = cs->info->ch;
			}
			*s++ = ')';
			MOWGLI_LIST_FOREACH(n, cu_pfx_list.head) {
				u_cu_pfx *cs = n->data;
				*s++ = cs->prefix;
			}
			*s++ = '\0';
		} else if (cur->s) {
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

	u_log(LG_DEBUG, "user: welcoming %s (auth=%s, class=%s)", u->nick,
	      u->link->conf.auth->name, u->link->conf.auth->cls->name);

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
	    IS_LOGGED_IN(u) ? u->acct : "*", u->gecos);
}

void u_user_flush_inputs(void)
{
	mowgli_patricia_iteration_state_t state;
	u_user *u;

	MOWGLI_PATRICIA_FOREACH(u, &state, users_by_uid) {
		if (u->link)
			u_link_flush_input(u->link);
	}
}

/* Serialization
 * -------------
 */
static int dump_specific_user(u_user *u, mowgli_json_t *j_users)
{
	mowgli_json_t *ju;

	if (!u->sv || !u->sv->sid[0]) {
		u_log(LG_WARN, "User on server without SID, ignoring");
		return 0;
	}

	ju = mowgli_json_create_object();
	json_oseto  (j_users, u->uid, ju);

	json_oseti  (ju, "mode",     u->mode);
	json_oseti  (ju, "flags",    u->flags);
	json_osets  (ju, "nick",     u->nick);
	json_osets  (ju, "acct",     u->acct);
	json_osettime(ju, "nickts", u->nickts);
	json_osets  (ju, "ident",    u->ident);
	json_osets  (ju, "ip",       u->ip);
	json_osets  (ju, "realhost", u->realhost);
	json_osets  (ju, "host",     u->host);
	json_osets  (ju, "gecos",    u->gecos);
	json_osets  (ju, "away",     u->away);
	json_oseto  (ju, "limit",    u_ratelimit_to_json(&u->limit));
	if (u->sv == &me) {
		/* Local user. */
		json_oseto  (ju, "link",     u_link_to_json(u->link));
	} else {
		/* Remote user; the link is serialized with the server.
		 * Reference it by SID. We can't infer this from the UID because our link
		 * to the user could be via an intermediate server. This is essentially the
		 * "next hop".
		 */
		json_osets  (ju, "link_via", u->sv->sid);
	}

	return 0;
}

int dump_user(void)
{
	int err;
	u_user *u;
	mowgli_patricia_iteration_state_t state;

	id_next();
	json_oseto(upgrade_json, "next_uid",
		mowgli_json_create_string_n(id_buf, strlen(id_buf)));

	/* Dump users */
	mowgli_json_t *j_users = mowgli_json_create_object();
	json_oseto(upgrade_json, "users", j_users);

	MOWGLI_PATRICIA_FOREACH(u, &state, users_by_uid) {
	  if ((err = dump_specific_user(u, j_users)) < 0)
	    return err;
	}

	return 0;
}

static int restore_specific_user(const char *uid, mowgli_json_t *ju)
{
	int err;
	u_user *u = NULL;
	u_link *link;
	mowgli_json_t *jl, *jlimit;
	mowgli_string_t
		*jslvia,
		*jsnick, *jsacct, *jsident,
		*jsip, *jsrealhost, *jshost, *jsgecos, *jsaway;
	u_server *sv, *sv_via;
	char sid[4];

	/* Get server for UID ------------------------- */
	if (strlen(uid) != 9)
		return -1;

	memcpy(sid, uid, 3);
	sid[3] = '\0';

	sv = u_server_by_sid(sid);
	if (!sv)
		return -1;

	/* Create user -------------------------------- */
	u = create_user(uid, NULL, sv);

	/* Fill in various fields --------------------- */
	if ((err = json_ogetu(ju, "mode", &u->mode)) < 0)
		return err;

	if ((err = json_ogetu(ju, "flags", &u->flags)) < 0)
		return err;

	if ((err = json_ogettime(ju, "nickts", &u->nickts)) < 0)
		return err;

	jsnick = json_ogets(ju, "nick");
	if (!jsnick || jsnick->pos > MAXNICKLEN)
		return -1;
	memcpy(u->nick,     jsnick->str,     jsnick->pos);

	jsacct = json_ogets(ju, "acct");
	if (!jsacct || jsnick->pos > MAXACCOUNT)
		return -1;
	memcpy(u->acct,     jsacct->str,     jsacct->pos);

	jsident = json_ogets(ju, "ident");
	if (!jsident || jsnick->pos > MAXIDENT)
		return -1;
	memcpy(u->ident,    jsident->str,    jsident->pos);

	jsip = json_ogets(ju, "ip");
	if (!jsip || jsip->pos > INET_ADDRSTRLEN)
		return -1;
	memcpy(u->ip,       jsip->str,       jsip->pos);

	jsrealhost = json_ogets(ju, "realhost");
	if (!jsrealhost || jsrealhost->pos > MAXHOST)
		return -1;
	memcpy(u->realhost, jsrealhost->str, jsrealhost->pos);

	jshost = json_ogets(ju, "host");
	if (!jshost || jshost->pos > MAXHOST)
		return -1;
	memcpy(u->host,     jshost->str,     jshost->pos);

	jsgecos = json_ogets(ju, "gecos");
	if (!jsgecos || jsgecos->pos > MAXGECOS)
		return -1;
	memcpy(u->gecos,    jsgecos->str,    jsgecos->pos);

	jsaway = json_ogets(ju, "away");
	if (!jsaway || jsaway->pos > MAXAWAY)
		return -1;
	memcpy(u->away,     jsaway->str,     jsaway->pos);

	jlimit = json_ogeto(ju, "limit");
	if (!jlimit)
		return -1;

	if ((err = u_ratelimit_from_json(jlimit, &u->limit)) < 0)
		return err;

	/* Create link -------------------------------- */
	err = -1;
	jl     = json_ogeto(ju, "link");
	jslvia = json_ogets(ju, "link_via");
	if (!!jl == !!jslvia) /* neither both nor neither */
		return -1;

	if (jl) {
		/* Local user */
		link = u_link_from_json(jl);
		if (!link)
			return -1;

		/* Can't fail now */
		u->link = link;
		u->link->priv = u;
	} else {
		/* Remote user */
		sv_via = u_server_by_sid(jslvia->str);
		if (!sv_via)
			return -1;

		u->link = sv_via->link;
	}

	mowgli_patricia_add(users_by_nick, u->nick, u);

	return 0;
}

int restore_user(void)
{
	int err;
	mowgli_json_t *ju, *jusers;
	mowgli_patricia_iteration_state_t state;
	mowgli_string_t *jsnextuid;
	const char *k;

	u_log(LG_DEBUG, "Restoring users...");

	/* Restore UID counter */
	jsnextuid = json_ogets(upgrade_json, "next_uid");
	if (!jsnextuid || jsnextuid->pos != 6) {
		u_log(LG_DEBUG, "no next UID? %u", (jsnextuid ? jsnextuid->pos : 9999));
		return -1;
	}

	if ((err = set_id_next_from_str(jsnextuid->str)) < 0)
		return -1;

	/* Restore users */
	jusers = json_ogeto_c(upgrade_json, "users");
	if (!jusers)
		return -1;

	MOWGLI_PATRICIA_FOREACH(ju, &state, MOWGLI_JSON_OBJECT(jusers)) {
		k = mowgli_patricia_elem_get_key(state.pspare[0]); /* XXX */
		if ((err = restore_specific_user(k, ju)) < 0)
			return err;
	}

	/* Done */
	u_log(LG_DEBUG, "Done restoring users");
	return 0;
}

/* Initialization
 * --------------
 */
int init_user(void)
{
	users_by_nick = mowgli_patricia_create(rfc1459_canonize);
	users_by_uid = mowgli_patricia_create(ascii_canonize);

	if (!users_by_nick || !users_by_uid)
		return -1;

	return 0;
}

/* vim: set noet: */

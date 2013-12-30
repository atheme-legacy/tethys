/* ircd-micro, server.c -- server data
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

u_trie *servers_by_sid;
u_trie *servers_by_name;

u_server me;
u_list my_motd;
char my_net_name[MAXNETNAME+1];
char my_admin_loc1[MAXADMIN+1] = "-";
char my_admin_loc2[MAXADMIN+1] = "-";
char my_admin_email[MAXADMIN+1] = "-";

void server_conf(char *key, char *val)
{
	if (strlen(key) < 3 || memcmp(key, "me.", 3)!=0) {
		u_log(LG_WARN, "server_conf: Can't use %s", key);
		return;
	}
	key += 3;

	if (streq(key, "name")) {
		u_trie_del(servers_by_name, me.name);
		u_strlcpy(me.name, val, MAXSERVNAME+1);
		u_trie_set(servers_by_name, me.name, &me);
		u_log(LG_DEBUG, "server_conf: me.name=%s", me.name);
	} else if (streq(key, "net")) {
		u_strlcpy(my_net_name, val, MAXNETNAME+1);
		u_log(LG_DEBUG, "server_conf: me.net=%s", my_net_name);
	} else if (streq(key, "sid")) {
		u_trie_del(servers_by_sid, me.sid);
		u_strlcpy(me.sid, val, 4);
		u_trie_set(servers_by_sid, me.sid, &me);
		u_log(LG_DEBUG, "server_conf: me.sid=%s", me.sid);
	} else if (streq(key, "desc")) {
		u_strlcpy(me.desc, val, MAXSERVDESC+1);
		u_log(LG_DEBUG, "server_conf: me.desc=%s", me.desc);
	} else {
		u_log(LG_WARN, "server_conf: Can't use %s", key-3);
	}
}

void load_motd(char *key, char *val)
{
	char *s, *p, buf[BUFSIZE];
	FILE *f;

	u_log(LG_INFO, "Loading MOTD from %s", val);

	f = fopen(val, "r");
	if (f == NULL) {
		u_log(LG_ERROR, "Could not open MOTD file %s", val);
		return;
	}

	while (!feof(f)) {
		s = fgets(buf, BUFSIZE, f);
		if (s == NULL)
			break;
		p = strchr(s, '\n');
		if (p) *p = '\0';
		u_log(LG_DEBUG, ":- %s", s);
		u_list_add(&my_motd, strdup(s));
	}

	fclose(f);
}

void admin_conf(char *key, char *val)
{
	char *dest;

	if (strlen(key) < 6 || memcmp(key, "admin.", 6) != 0) {
		u_log(LG_WARN, "admin_conf: Can't use %s", key);
		return;
	}
	key += 6;

	if (streq(key, "loc1")) {
		dest = my_admin_loc1;
	} else if (streq(key, "loc2")) {
		dest = my_admin_loc2;
	} else if (streq(key, "email")) {
		dest = my_admin_email;
	} else {
		u_log(LG_WARN, "admin_conf: Can't use %s", key-6);
		return;
	}

	u_strlcpy(dest, val, MAXADMIN);
}

u_server *u_server_by_sid(char *sid)
{
	return u_trie_get(servers_by_sid, sid);
}

u_server *u_server_by_name(char *name)
{
	return u_trie_get(servers_by_name, name);
}

u_server *u_server_find(char *str)
{
	if (strchr(str, '.'))
		return u_server_by_name(str);
	return u_server_by_sid(str);
}

struct capab_info {
	char capab[16];
	uint mask;
} capabs[] = {
	{ "QS",       CAPAB_QS       },
	{ "EX",       CAPAB_EX       },
	{ "CHW",      CAPAB_CHW      },
	{ "IE",       CAPAB_IE       },
	{ "EOB",      CAPAB_EOB      },
	{ "KLN",      CAPAB_KLN      },
	{ "UNKLN",    CAPAB_UNKLN    },
	{ "KNOCK",    CAPAB_KNOCK    },
	{ "TB",       CAPAB_TB       },
	{ "ENCAP",    CAPAB_ENCAP    },
	{ "SERVICES", CAPAB_SERVICES },
	{ "SAVE",     CAPAB_SAVE     },
	{ "RSFNC",    CAPAB_RSFNC    },
	{ "EUID",     CAPAB_EUID     },
	{ "CLUSTER",  CAPAB_CLUSTER  },
	{ "", 0 }
};

static void capab_to_str(uint capab, char *buf)
{
	struct capab_info *info = capabs;
	char *s = buf;

	for (; info->capab[0]; info++) {
		if (!(capab & info->mask))
			continue;
		s += sprintf(s, "%s%s", s == buf ? "" : " ", info->capab);
	}
}

static uint str_to_capab(char *buf)
{
	struct capab_info *info;
	char *s;
	uint capab = 0;

	while ((s = cut(&buf, " ")) != NULL) {
		for (info=capabs; info->capab[0]; info++) {
			if (streq(info->capab, s))
				break;
		}
		if (!info->capab[0])
			continue;
		capab |= info->mask;
	}

	return capab;
}

void u_server_add_capabs(u_server *sv, char *s)
{
	sv->capab |= str_to_capab(s);
}

void u_my_capabs(char *buf)
{
	capab_to_str(me.capab, buf);
}

void server_local_die(u_conn *conn, char *msg)
{
	u_server *sv = conn->priv;
	if (sv == NULL)
		return;
	if (conn->ctx == CTX_SERVER) {
		/* TODO: send SQUIT or something */
	}
	u_conn_f(conn, "ERROR :%s", msg);
	u_server_unlink(sv);
}

void server_local_event(u_conn *conn, int event)
{
	switch (event) {
	case EV_ERROR:
		server_local_die(conn, conn->error);
		break;
	default:
		server_local_die(conn, "unknown error");
		break;

	case EV_DESTROYING:
		break;
	}
}

void u_server_make_sreg(u_conn *conn, char *sid)
{
	u_server *sv;

	if (conn->ctx != CTX_UNREG && conn->ctx != CTX_SREG)
		return;

	conn->ctx = CTX_SREG;

	if (conn->priv != NULL)
		return;

	conn->priv = sv = malloc(sizeof(*sv));
	sv->conn = conn;
	sv->flags = SERVER_IS_BURSTING;

	strcpy(sv->sid, sid);
	u_trie_set(servers_by_sid, sv->sid, sv);

	sv->name[0] = '\0';
	sv->desc[0] = '\0';
	sv->conn = conn;
	sv->capab = 0;

	sv->hops = 1;
	sv->parent = &me;

	sv->nusers = 0;
	sv->nlinks = 0;

	conn->event = server_local_event;

	u_log(LG_INFO, "New local server sid=%s", sv->sid);

	sv->parent->nlinks++;
}

u_server *u_server_new_remote(u_server *parent, char *sid,
                              char *name, char *desc)
{
	u_server *sv;

	sv = malloc(sizeof(*sv));

	sv->conn = parent->conn;
	strcpy(sv->sid, sid);
	u_strlcpy(sv->name, name, MAXSERVNAME+1);
	u_strlcpy(sv->desc, desc, MAXSERVDESC+1);
	sv->capab = 0;
	sv->hops = parent->hops + 1;
	sv->parent = parent;

	sv->nusers = 0;
	sv->nlinks = 0;

	u_trie_set(servers_by_sid, sv->sid, sv);
	u_trie_set(servers_by_name, sv->name, sv);

	u_log(LG_INFO, "New remote server sid=%s", sv->sid);

	sv->parent->nlinks++;

	return sv;
}

static void delete_links(u_server *tsv, u_server *sv)
{
	if (tsv->parent == sv)
		u_server_unlink(tsv);
}

static void user_delete(u_user *u, void *priv)
{
	u_sendto_visible(u, ST_USERS, ":%H QUIT :*.net *.split", u);
	u_user_unlink(u);
}

void u_server_unlink(u_server *sv)
{
	if (sv == &me) {
		u_log(LG_ERROR, "Can't unlink self!");
		return;
	}

	u_log(LG_INFO, "Unlinking server sid=%s (%S)", sv->sid, sv);

	if (IS_SERVER_LOCAL(sv)) {
		u_conn *conn = sv->conn;
		u_roster_del_all(conn);
		conn->ctx = CTX_CLOSED;
		conn->priv = NULL;
	}

	sv->parent->nlinks--;

	/* delete all users */
	u_trie_each(users_by_uid, sv->sid, (u_trie_cb_t*)user_delete, NULL);

	if (sv->name[0])
		u_trie_del(servers_by_name, sv->name);
	u_trie_del(servers_by_sid, sv->sid);

	/* delete any servers linked to this one */
	u_trie_each(servers_by_sid, NULL, (u_trie_cb_t*)delete_links, sv);

	free(sv);
}

static void burst_euid(u_user *u, u_conn *conn)
{
	char buf[512];

	u_user_make_euid(u, buf);
	u_conn_f(conn, "%s", buf);

	if (u->away[0])
		u_conn_f(conn, ":%U AWAY :%s", u, u->away);
}

static void burst_uid(u_user *u, u_conn *conn)
{
	u_server *sv = u_user_server(u);

	/* NOTE: this is legacy, but I'm keeping it around anyway */

	/* EQUALLY RIDICULOUS!  nick     modes    ip
                                   hops     ident    uid
                                      nickts   host     gecos    */
	u_conn_f(conn, ":%S UID %s %d %u %s %s %s %s %s :%s",
	         sv,
	         u->nick, sv->hops + 1, u->nickts,
	         "+", u->ident, u->host,
	         u->ip, u->uid, u->gecos);

	u_conn_f(conn, ":%U ENCAP * REALHOST :%s", u, u->realhost);

	if (u->acct[0])
		u_conn_f(conn, ":%U ENCAP * LOGIN :%s", u, u->acct);

	u_conn_f(conn, ":%U AWAY :%s", u, u->away);
}

struct burst_chan_priv {
	u_chan *c;
	u_conn *conn;
	char *rest, *s, buf[512];
	uint w;
};

static void burst_chan_cb(u_map *map, u_user *u, u_chanuser *cu,
                          struct burst_chan_priv *priv)
{
	char *p, nbuf[12];

	p = nbuf;
	if (cu->flags & CU_PFX_OP)
		*p++ = '@';
	if (cu->flags & CU_PFX_VOICE)
		*p++ = '+';
	strcpy(p, u->uid);

try_again:
	if (!wrap(priv->buf, &priv->s, priv->w, nbuf)) {
		u_conn_f(priv->conn, "%s%s", priv->rest, priv->buf);
		goto try_again;
	}
}

static void burst_chan(u_chan *c, u_conn *conn)
{
	struct burst_chan_priv priv;
	char buf[512];
	int sz;

	sz = snf(FMT_SERVER, buf, 512, ":%S SJOIN %u %s %s :",
	         &me, c->ts, c->name, u_chan_modes(c, 1));

	priv.c = c;
	priv.conn = conn;
	priv.rest = buf;
	priv.s = priv.buf;
	priv.w = 512 - sz;

	u_map_each(c->members, (u_map_cb_t*)burst_chan_cb, &priv);
	if (priv.s != priv.buf)
		u_conn_f(conn, "%s%s", buf, priv.buf);
}

void u_server_burst(u_server *sv, u_link *link)
{
	u_conn *conn = sv->conn;
	char buf[512];

	if (conn == NULL) {
		u_log(LG_ERROR, "Attempted to burst to %S, which has no conn!", sv);
		return;
	}

	if (sv->hops != 1) {
		u_log(LG_ERROR, "Attempted to burst to %S, which is not local!", sv);
		return;
	}

	conn->ctx = CTX_SERVER;

	u_conn_f(conn, "PASS %s TS 6 :%s", link->sendpass, me.sid);
	u_my_capabs(buf);
	u_conn_f(conn, "CAPAB :%s", buf);
	u_conn_f(conn, "SERVER %s 1 :%s", me.name, me.desc);

	u_conn_f(conn, "SVINFO 6 6 0 :%u", NOW.tv_sec);

	/* TODO: "SID and SERVER messages for all known servers" */

	/* TODO: "BAN messages for all propagated bans" */

	/* TODO: "EUID for all known users (possibly followed by ENCAP
	   REALHOST, ENCAP LOGIN, and/or AWAY)" */
	if (sv->capab & CAPAB_EUID)
		u_trie_each(users_by_uid, NULL, (u_trie_cb_t*)burst_euid, conn);
	else
		u_trie_each(users_by_uid, NULL, (u_trie_cb_t*)burst_uid, conn);

	/* TODO: "and SJOIN messages for all known channels (possibly followed
	   by BMASK and/or TB)" */
	u_trie_each(all_chans, NULL, (u_trie_cb_t*)burst_chan, conn);

	u_conn_f(conn, ":%S PING %s %s", &me, me.name, sv->name);

	u_log(LG_DEBUG, "Adding %s to servers_by_name", sv->name);
	u_trie_set(servers_by_name, sv->name, sv);
}

void u_server_eob(u_server *sv)
{
	if (sv->hops != 1) {
		u_log(LG_ERROR, "u_server_eob called for remote server %S!", sv);
		return;
	}

	u_log(LG_VERBOSE, "End of burst with %S", sv);
	sv->flags &= ~SERVER_IS_BURSTING;
	u_roster_add(R_SERVERS, sv->conn);
}

int init_server(void)
{
	servers_by_sid = u_trie_new(ascii_canonize);
	servers_by_name = u_trie_new(ascii_canonize);

	/* default settings! */
	me.conn = NULL;
	strcpy(me.sid, "22U");
	u_strlcpy(me.name, "micro.irc", MAXSERVNAME+1);
	u_strlcpy(me.desc, "The Tiny IRC Server", MAXSERVDESC+1);
	me.capab = CAPAB_QS | CAPAB_EX | CAPAB_CHW | CAPAB_IE
	         | CAPAB_EOB | CAPAB_KLN | CAPAB_UNKLN | CAPAB_KNOCK
	         | CAPAB_TB | CAPAB_ENCAP | CAPAB_SERVICES
	         | CAPAB_SAVE | CAPAB_EUID;
	me.hops = 0;
	me.parent = NULL;

	me.nusers = 0;
	me.nlinks = 0;

	u_trie_set(servers_by_name, me.name, &me);
	u_trie_set(servers_by_sid, me.sid, &me);

	u_list_init(&my_motd);

	u_strlcpy(my_net_name, "MicroIRC", MAXNETNAME+1);

	u_trie_set(u_conf_handlers, "me.name", server_conf);
	u_trie_set(u_conf_handlers, "me.net", server_conf);
	u_trie_set(u_conf_handlers, "me.sid", server_conf);
	u_trie_set(u_conf_handlers, "me.desc", server_conf);
	u_trie_set(u_conf_handlers, "me.motd", load_motd);

	u_trie_set(u_conf_handlers, "admin.loc1", admin_conf);
	u_trie_set(u_conf_handlers, "admin.loc2", admin_conf);
	u_trie_set(u_conf_handlers, "admin.email", admin_conf);

	return 1;
}

/* Tethys, server.c -- server data
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

mowgli_patricia_t *servers_by_sid;
mowgli_patricia_t *servers_by_name;

u_server me;
mowgli_list_t my_motd;
mowgli_list_t my_admininfo;
char my_net_name[MAXNETNAME+1];

static void load_motd(char *val)
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
		mowgli_list_add(&my_motd, strdup(s));
	}

	fclose(f);
}

static void server_conf(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	mowgli_config_file_entry_t *cce;

	MOWGLI_ITER_FOREACH(cce, ce->entries) {
		if (streq(cce->varname, "name")) {
			mowgli_patricia_delete(servers_by_name, me.name);
			u_strlcpy(me.name, cce->vardata, MAXSERVNAME+1);
			mowgli_patricia_add(servers_by_name, me.name, &me);
			u_log(LG_DEBUG, "server_conf: me.name=%s", me.name);
		} else if (streq(cce->varname, "net")) {
			u_strlcpy(my_net_name, cce->vardata, MAXNETNAME+1);
			u_log(LG_DEBUG, "server_conf: me.net=%s", my_net_name);
		} else if (streq(cce->varname, "sid")) {
			mowgli_patricia_delete(servers_by_sid, me.sid);
			u_strlcpy(me.sid, cce->vardata, 4);
			mowgli_patricia_add(servers_by_sid, me.sid, &me);
			u_log(LG_DEBUG, "server_conf: me.sid=%s", me.sid);
		} else if (streq(cce->varname, "desc")) {
			u_strlcpy(me.desc, cce->vardata, MAXSERVDESC+1);
			u_log(LG_DEBUG, "server_conf: me.desc=%s", me.desc);
		} else if (streq(cce->varname, "motd")) {
			load_motd(cce->vardata);
		} else {
			u_log(LG_WARN, "server_conf: Can't use %s", cce->varname);
		}
	}
}

static void admin_conf(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	mowgli_config_file_entry_t *cce;

	MOWGLI_ITER_FOREACH(cce, ce->entries) {
		mowgli_list_add(&my_admininfo, strdup(cce->varname));
	}
}

u_server *u_server_by_sid(const char *sid)
{
	return mowgli_patricia_retrieve(servers_by_sid, sid);
}

u_server *u_server_by_name(const char *name)
{
	return mowgli_patricia_retrieve(servers_by_name, name);
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

void u_server_make_sreg(u_link *link, char *sid)
{
	u_server *sv;

	if (link->type != LINK_NONE && link->type != LINK_SERVER)
		return;

	link->type = LINK_SERVER;

	if (link->priv != NULL)
		return;

	link->priv = sv = malloc(sizeof(*sv));
	sv->link = link;
	sv->flags = SERVER_IS_BURSTING;

	u_strlcpy(sv->sid, sid, 4);
	mowgli_patricia_add(servers_by_sid, sv->sid, sv);

	sv->name[0] = '\0';
	sv->desc[0] = '\0';
	sv->link = link;
	sv->capab = 0;

	sv->hops = 1;
	sv->parent = &me;

	sv->nusers = 0;
	sv->nlinks = 0;

	u_log(LG_INFO, "New local server sid=%s", sv->sid);

	sv->parent->nlinks++;
}

u_server *u_server_new_remote(u_server *parent, char *sid,
                              char *name, char *desc)
{
	u_server *sv;

	sv = malloc(sizeof(*sv));

	sv->link = parent->link;
	if (sid)
		u_strlcpy(sv->sid, sid, 4);
	else
		sv->sid[0] = '\0'; /* TS5 */
	u_strlcpy(sv->name, name, MAXSERVNAME+1);
	u_strlcpy(sv->desc, desc, MAXSERVDESC+1);
	sv->capab = 0;
	sv->hops = parent->hops + 1;
	sv->parent = parent;

	sv->nusers = 0;
	sv->nlinks = 0;

	if (sv->sid[0])
		mowgli_patricia_add(servers_by_sid, sv->sid, sv);
	mowgli_patricia_add(servers_by_name, sv->name, sv);

	u_log(LG_INFO, "New remote server name=%s, sid=%s", sv->name, sv->sid);

	sv->parent->nlinks++;

	return sv;
}

void u_server_destroy(u_server *sv)
{
	mowgli_patricia_iteration_state_t state;
	u_user *u;
	u_server *tsv;

	if (sv == &me) {
		u_log(LG_ERROR, "Can't unlink self!");
		return;
	}

	u_log(LG_INFO, "Unlinking server sid=%s (%S)", sv->sid, sv);

	sv->parent->nlinks--;

	/* delete all users */
	if (sv->sid[0]) {
		MOWGLI_PATRICIA_FOREACH(u, &state, users_by_uid) {
			if (strncmp(sv->sid, u->uid, 3))
				continue;

			u_sendto_visible(u, ST_USERS, ":%H QUIT :*.net *.split", u);
			u_user_destroy(u);
		}
	}

	if (sv->name[0])
		mowgli_patricia_delete(servers_by_name, sv->name);
	if (sv->sid[0])
		mowgli_patricia_delete(servers_by_sid, sv->sid);

	/* delete any servers linked to this one */
	MOWGLI_PATRICIA_FOREACH(tsv, &state, servers_by_sid) {
		if (tsv->parent == sv)
			u_server_destroy(tsv);
	}

	free(sv);
}

static int burst_euid(const char *key, void *value, void *priv)
{
	u_user *u = value;
	u_link *link = priv;
	char buf[512];

	u_user_make_euid(u, buf);
	u_link_f(link, "%s", buf);

	if (IS_AWAY(u))
		u_link_f(link, ":%U AWAY :%s", u, u->away);

	return 0;
}

static int burst_uid(const char *key, void *value, void *priv)
{
	u_user *u = value;
	u_link *link = priv;

	/* NOTE: this is legacy, but I'm keeping it around anyway */

	/* EQUALLY RIDICULOUS!  nick     modes    ip
                                   hops     ident    uid
                                      nickts   host     gecos    */
	u_link_f(link, ":%S UID %s %d %u %s %s %s %s %s :%s",
	         u->sv,
	         u->nick, u->sv->hops + 1, u->nickts,
	         "+", u->ident, u->host,
	         u->ip, u->uid, u->gecos);

	u_link_f(link, ":%U ENCAP * REALHOST :%s", u, u->realhost);

	if (IS_LOGGED_IN(u))
		u_link_f(link, ":%U ENCAP * LOGIN :%s", u, u->acct);

	if (IS_AWAY(u))
		u_link_f(link, ":%U AWAY :%s", u, u->away);

	return 0;
}

static int burst_chan(const char *key, void *_c, void *_link)
{
	u_chan *c = _c;
	u_link *link = _link;
	u_user *u;
	u_chanuser *cu;
	u_map_each_state st;
	u_strop_wrap wrap;
	mowgli_node_t *n;
	char *s, buf[512];
	int sz;

	if (c->flags & CHAN_LOCAL)
		return 0;

	sz = snf(FMT_SERVER, buf, 512, ":%S SJOIN %u %s %s :",
	         &me, c->ts, c->name, u_chan_modes(c, 1));

	u_strop_wrap_start(&wrap, 510 - sz);
	U_MAP_EACH(&st, c->members, &u, &cu) {
		char *p, nbuf[12];

		p = nbuf;
		MOWGLI_LIST_FOREACH(n, cu_pfx_list.head) {
			u_cu_pfx *cs = n->data;
			if (cu->flags & cs->mask)
				*p++ = cs->prefix;
		}
		strcpy(p, u->uid);

		while ((s = u_strop_wrap_word(&wrap, nbuf)) != NULL)
			u_link_f(link, "%s%s", buf, s);
	}
	if ((s = u_strop_wrap_word(&wrap, NULL)) != NULL)
		u_link_f(link, "%s%s", buf, s);

	if (c->topic[0]) {
		u_link_f(link, ":%S TB %C %u %s :%s", &me, c,
		         c->topic_time, c->topic_setter, c->topic);
	}

	return 0;
}

void u_server_burst_1(u_link *link, u_link_block *block)
{
	char buf[512];

	if (!link)
		return;

	if (link->flags & U_LINK_SENT_PASS)
		return;

	link->flags |= U_LINK_SENT_PASS;

	u_link_f(link, "PASS %s TS 6 :%s", block->sendpass, me.sid);
	u_my_capabs(buf);
	u_link_f(link, "CAPAB :%s", buf);
	u_link_f(link, "SERVER %s 1 :%s", me.name, me.desc);
}

void u_server_burst_2(u_server *sv, u_link_block *block)
{
	mowgli_patricia_iteration_state_t state;
	u_server *tsv;
	u_link *link = sv->link;

	if (link == NULL) {
		u_log(LG_ERROR, "Attempted to burst to %S, which has no link!", sv);
		return;
	}

	if (sv->hops != 1) {
		u_log(LG_ERROR, "Attempted to burst to %S, which is not local!", sv);
		return;
	}

	u_link_f(link, "SVINFO 6 6 0 :%u", NOW.tv_sec);

	MOWGLI_PATRICIA_FOREACH(tsv, &state, servers_by_name) {
		if (tsv == &me)
			continue;

		if (tsv->sid[0]) {
			u_link_f(link, ":%S SID %s %d %s :%s", tsv->parent,
			         tsv->name, tsv->hops, tsv->sid, tsv->desc);
		} else {
			u_link_f(link, ":%S SERVER %s %d :%s", tsv->parent,
			         tsv->name, tsv->hops, tsv->desc);
		}
	}

	/* TODO: "BAN messages for all propagated bans" */

	/* TODO: "EUID for all known users (possibly followed by ENCAP
	   REALHOST, ENCAP LOGIN, and/or AWAY)" */
	if (sv->capab & CAPAB_EUID)
		mowgli_patricia_foreach(users_by_uid, burst_euid, link);
	else
		mowgli_patricia_foreach(users_by_uid, burst_uid, link);

	/* TODO: "and SJOIN messages for all known channels (possibly followed
	   by BMASK and/or TB)" */
	mowgli_patricia_foreach(all_chans, burst_chan, link);

	u_link_f(link, ":%S PING %s %s", &me, me.name, sv->name);

	u_log(LG_DEBUG, "Adding %s to servers_by_name", sv->name);
	mowgli_patricia_add(servers_by_name, sv->name, sv);
}

void u_server_eob(u_server *sv)
{
	if (sv->hops != 1) {
		u_log(LG_ERROR, "u_server_eob called for remote server %S!", sv);
		return;
	}

	u_log(LG_VERBOSE, "End of burst with %S", sv);
	sv->flags &= ~SERVER_IS_BURSTING;
}

void u_server_flush_inputs(void)
{
	mowgli_patricia_iteration_state_t state;
	u_server *s;

	MOWGLI_PATRICIA_FOREACH(s, &state, servers_by_sid) {
		if (s->link)
			u_link_flush_input(s->link);
	}
}

/* Serialization
 * -------------
 */
static int dump_specific_server(u_server *s, mowgli_json_t *j_servers)
{
	mowgli_json_t *js;

	if (!s->sid[0])
		/* A server entry is created for juped servers, but with no sid.
		 * Just ignore it, it'll be recreated from the config file.
		 */
		return 0;

	js = mowgli_json_create_object();
	json_oseto  (j_servers, s->sid, js);

	json_osets  (js, "name", s->name);
	json_osets  (js, "desc", s->desc);
	json_oseti  (js, "capab", s->capab);
	json_oseti  (js, "hops",  s->hops);
	json_osets  (js, "parent", (s->parent) ? s->parent->sid : NULL);
	json_oseto  (js, "link", u_link_to_json(s->link));
	json_osetu  (js, "nlinks", s->nlinks);

	return 0;
}

int dump_server(void)
{
	int err;
	mowgli_patricia_iteration_state_t state;
	u_server *s;

	mowgli_json_t *j_servers = mowgli_json_create_object();
	json_oseto(upgrade_json, "servers", j_servers);

	MOWGLI_PATRICIA_FOREACH(s, &state, servers_by_sid) {
		if ((err = dump_specific_server(s, j_servers)) < 0)
			return err;
	}

	return 0;
}

static int restore_specific_server(const char *sid, mowgli_json_t *js)
{
	static bool _restored_me = false;
	u_server *s       = NULL;
	u_server *sparent = NULL;
	u_link *link      = NULL;
	char psid[4]      = {};
	mowgli_string_t *jsparent, *jsname, *jsdesc;
	mowgli_json_t   *jlink;

	if (strlen(sid) != 3)
		return -1;

	if (!memcmp(sid, me.sid, 3))
		s = &me;

	/* We already have this server. But since we've already read the
	 * configuration file, 'me' will be fully loaded and in the lookup tables.
	 * So we only read a few fields when s == &me.
	 */
	if (u_server_by_sid(sid) && (s != &me || _restored_me))
		return 0;

	/* Parent ----------------------------- */
	jsparent = json_ogets(js, "parent");
	if (jsparent) {
		if (s == &me || jsparent->pos != 3)
			return -1;

		memcpy(psid, jsparent->str, 3);
		sparent = u_server_by_sid(psid);

		/* Come back to this later if our parent isn't added yet. */
		if (!sparent)
			return 0;
	} 

	u_log(LG_DEBUG, "Restoring server [%s]", sid);
	jlink = json_ogeto(js, "link");

	if (!s)
		s = calloc(1, sizeof(*s));

	if (json_ogetu(js, "hops", &s->hops) < 0)
		return -1;

	if (json_ogetu(js, "capab", &s->capab) < 0)
		return -1;

	if (json_ogetu(js, "nlinks", &s->nlinks) < 0)
		return -1;

	if (s == &me) {
		/* name and description must match what is in the
		 * configuration file for 'me'
		 */
		if (jlink)
			return -1;

		_restored_me = true;
	} else {
		if (!jlink)
			return -1;

		link = u_link_from_json(jlink);
		if (!link)
			return -1;

		memcpy(s->sid, sid, 3);
		s->link = link;
		s->link->priv = s;
		s->parent = sparent;

		jsname = json_ogets(js, "name");
		if (!jsname || jsname->pos > MAXSERVNAME)
			return -1;
		memcpy(s->name, jsname->str, jsname->pos);

		jsdesc = json_ogets(js, "desc");
		if (!jsdesc || jsname->pos > MAXSERVDESC)
			return -1;
		memcpy(s->desc, jsdesc->str, jsdesc->pos);

		mowgli_patricia_add(servers_by_sid,  s->sid,  s);
		mowgli_patricia_add(servers_by_name, s->name, s);
	}

	return 1;
}

int restore_server(void)
{
	int err;
	mowgli_json_t *jss, *js;
	mowgli_patricia_iteration_state_t state;
	int num_added;
	const char *k;

	u_log(LG_DEBUG, "Restoring servers...");

	/* Add servers */
	jss = json_ogeto_c(upgrade_json, "servers");
	if (!jss)
		return -1;

	do {
		num_added = 0;
		MOWGLI_PATRICIA_FOREACH(js, &state, MOWGLI_JSON_OBJECT(jss)) {
			k = mowgli_patricia_elem_get_key(state.pspare[0]); /* XXX */
			err = restore_specific_server(k, js);
			if (err < 0)
				return err;
			if (err > 0)
				++num_added;
		}
	} while (num_added);

	u_log(LG_DEBUG, "Done restoring servers");
	return 0;
}

/* Unit Initialization
 * -------------------
 */
int init_server(void)
{
	servers_by_sid = mowgli_patricia_create(ascii_canonize);
	servers_by_name = mowgli_patricia_create(ascii_canonize);

	mowgli_list_init(&my_motd);

	u_strlcpy(my_net_name, "TethysIRC", MAXNETNAME+1);

	u_conf_add_handler("me", server_conf, NULL);
	u_conf_add_handler("admin", admin_conf, NULL);

	/* default settings! */
	me.link = NULL;
	strcpy(me.sid, "22U");
	u_strlcpy(me.name, "tethys.irc", MAXSERVNAME+1);
	u_strlcpy(me.desc, "The Tiny IRC Server", MAXSERVDESC+1);
	me.capab = CAPAB_QS | CAPAB_EX | CAPAB_CHW | CAPAB_IE
	         | CAPAB_EOB | CAPAB_KLN | CAPAB_UNKLN | CAPAB_KNOCK
	         | CAPAB_TB | CAPAB_ENCAP | CAPAB_SERVICES
	         | CAPAB_SAVE | CAPAB_EUID;
	me.hops = 0;
	me.parent = NULL;

	me.nusers = 0;
	me.nlinks = 0;

	mowgli_patricia_add(servers_by_name, me.name, &me);
	mowgli_patricia_add(servers_by_sid, me.sid, &me);

	return 1;
}

/* vim: set noet: */

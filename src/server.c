/* ircd-micro, server.c -- server data
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

struct u_server me;
struct u_list my_motd;

void server_conf(key, val)
char *key, *val;
{
	if (strlen(key) < 3 || memcmp(key, "me.", 3)!=0) {
		u_log(LG_WARN, "server_conf: Can't use %s", key);
		return;
	}
	key += 3;

	if (!strcmp(key, "name")) {
		u_strlcpy(me.name, val, MAXSERVNAME+1);
		u_log(LG_VERBOSE, "server_conf: me.name=%s", me.name);
	} else if (!strcmp(key, "sid")) {
		u_strlcpy(me.sid, val, 4);
		u_log(LG_VERBOSE, "server_conf: me.sid=%s", me.sid);
	} else if (!strcmp(key, "desc")) {
		u_strlcpy(me.desc, val, MAXSERVDESC+1);
		u_log(LG_VERBOSE, "server_conf: me.desc=%s", me.desc);
	} else {
		u_log(LG_WARN, "server_conf: Can't use %s", key-3);
	}
}

void load_motd(key, val)
char *key, *val;
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
		u_list_add(&my_motd, u_strdup(s));
	}

	fclose(f);
}

int init_server()
{
	u_list_init(&my_motd);

	/* default settings! */
	me.conn = NULL;
	strcpy(me.sid, "22U");
	u_strlcpy(me.name, "micro.irc", MAXSERVNAME+1);
	u_strlcpy(me.desc, "The Tiny IRC Server", MAXSERVDESC+1);

	u_trie_set(u_conf_handlers, "me.name", server_conf);
	u_trie_set(u_conf_handlers, "me.sid", server_conf);
	u_trie_set(u_conf_handlers, "me.desc", server_conf);
	u_trie_set(u_conf_handlers, "me.motd", load_motd);

	return 1;
}

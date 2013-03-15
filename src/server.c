#include "ircd.h"

struct u_server me;

void server_conf(key, val)
char *key, *val;
{
	if (strlen(key) < 3 || memcmp(key, "me.", 3)!=0) {
		u_log("server_conf: Can't use %s\n", key);
		return;
	}
	key += 3;

	if (!strcmp(key, "name")) {
		u_strlcpy(me.name, val, MAXSERVNAME+1);
		u_debug("server_conf: me.name=%s\n", me.name);
	} else if (!strcmp(key, "sid")) {
		u_strlcpy(me.sid, val, 4);
		u_debug("server_conf: me.sid=%s\n", me.sid);
	} else if (!strcmp(key, "desc")) {
		u_strlcpy(me.desc, val, MAXSERVDESC+1);
		u_debug("server_conf: me.desc=%s\n", me.desc);
	} else {
		u_log("server_conf: Can't use %s\n", key-3);
	}
}

int init_server()
{
	/* default settings! */
	me.conn = NULL;
	strcpy(me.sid, "22U");
	u_strlcpy(me.name, "micro.irc", MAXSERVNAME+1);
	u_strlcpy(me.desc, "The Tiny IRC Server", MAXSERVDESC+1);

	u_trie_set(u_conf_handlers, "me.name", server_conf);
	u_trie_set(u_conf_handlers, "me.sid", server_conf);
	u_trie_set(u_conf_handlers, "me.desc", server_conf);

	return 1;
}

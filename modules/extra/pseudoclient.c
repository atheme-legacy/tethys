/* Tethys, pseudoclient.c -- server pseudoclient
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_user_local *pseudo = NULL;

static char *pseudo_create_nick(void)
{
	static char buf[10];
	snprintf(buf, 10, "%s-ircd", me.sid);
	return buf;
}

static void pseudo_nick(char *nick)
{
	u_user *u = USER(pseudo);

	if (!strcmp(u->nick, nick))
		return;

	u_sendto_visible(u, ST_USERS, ":%H NICK :%s", u, nick);
	u_roster_f(R_SERVERS, NULL, ":%H NICK %s %u", u, nick, 0);

	u_user_set_nick(USER(pseudo), nick, 0);

	u_hook_call("pseudoclient:update", pseudo);
}

static void pseudo_quit(const char *reason)
{
	u_user *u = USER(pseudo);

	u_sendto_visible(u, ST_USERS, ":%H QUIT :%s", u, reason);
	u_roster_f(R_SERVERS, NULL, ":%H QUIT :%s", u, reason);

	u_hook_call("pseudoclient:destroy", pseudo);

	u_user_unlink(USER(pseudo));
}

static void try_add_pseudo(void)
{
	if (pseudo || !me.sid[0] || !me.name[0])
		return;

	pseudo = u_user_local_create("127.0.0.1", me.name);
	strcpy(pseudo->user.ident, "ircd");
	strcpy(pseudo->user.gecos, "IRCD pseudoclient");
	u_user_set_nick(USER(pseudo), pseudo_create_nick(), 0);
	u_strlcpy(pseudo->user.host, me.name, MAXHOST+1);

	u_hook_call("pseudoclient:create", pseudo);
}

static void *on_conf_end(void *a, void *b)
{
	char *nick;

	try_add_pseudo();

	nick = pseudo_create_nick();
	if (pseudo && strcmp(nick, pseudo->user.nick))
		pseudo_nick(nick);
}

static int pseudoclient_init(u_module *m)
{
	try_add_pseudo();

	u_hook_add(HOOK_CONF_END, on_conf_end, NULL);
}

static void pseudoclient_deinit(u_module *m)
{
	if (pseudo != NULL) {
		pseudo_quit("Module unloaded");
		pseudo = NULL;
	}
}

TETHYS_MODULE_V1(
	"extra/pseudoclient",
	"Alex Iadicicco <http://github.com/aji>",
	"IRCD pseudoclient",

	pseudoclient_init,
	pseudoclient_deinit
);

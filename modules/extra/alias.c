/* Tethys, alias.c -- command alias support
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

struct alias {
	u_cmd cmd;
	char target[MAXNICKLEN+1];
};

static mowgli_patricia_t *aliases;

static int m_alias(u_sourceinfo *si, u_msg *msg)
{
	struct alias *to;
	char line[512];
	int i, len;
	u_user *tu;

	if (!SRC_IS_USER(si)) /* needed? */
		return 0;

	if (!(to = mowgli_patricia_retrieve(aliases, msg->command)))
		return 0;

	if (!(tu = u_user_by_nick(to->target)))
		goto unavailable;
	if (IS_LOCAL_USER(tu)) /* TODO: check +S instead */
		goto unavailable;

	len = 0;
	for (i=0; i<msg->argc && len+1<512; i++) {
		len += snprintf(line + len, 512 - len, "%s%s",
		                i ? " " : "", msg->argv[i]);
	}

	u_conn_f(tu->link, ":%H PRIVMSG %U :%s", si->u, tu, line);

	return 0;

unavailable:
	u_conn_num(si->link, ERR_SERVICESDOWN, to->target);
	return 0;
}

static void add_alias(char *from, char *target)
{
	struct alias *to;

	if (mowgli_patricia_retrieve(aliases, from)) {
		u_log(LG_ERROR, "Alias %s=%s is a duplicate", from, target);
		return;
	}

	to = calloc(1, sizeof(*to));

	u_strlcpy(to->cmd.name, from, MAXCOMMANDLEN+1);
	ascii_canonize(to->cmd.name);
	to->cmd.mask = SRC_USER;
	to->cmd.cb = m_alias;
	to->cmd.nargs = 1;

	u_strlcpy(to->target, target, MAXNICKLEN+1);

	if (u_cmd_reg(&to->cmd) < 0) {
		u_log(LG_ERROR, "%s alias registration failed", to->cmd.name);
		free(to);
		return;
	}

	u_log(LG_INFO, "Created alias %s=%s", to->cmd.name, to->target);

	mowgli_patricia_add(aliases, from, to);
}

static void conf_alias(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	char *src = ce->vardata;
	char *dst = NULL;
	mowgli_config_file_entry_t *cce;

	MOWGLI_ITER_FOREACH(cce, ce->entries) {
		if (streq(cce->varname, "target"))
			dst = cce->vardata;
	}

	if (dst == NULL) {
		u_log(LG_ERROR, "%s no target specified", src);
		return;
	}

	add_alias(src, dst);
}

static int alias_init(u_module *m)
{
	aliases = mowgli_patricia_create(ascii_canonize);
	u_conf_add_handler("alias", conf_alias, NULL);

	return 0;
}

static void alias_deinit(u_module *m)
{
	mowgli_patricia_iteration_state_t state;
	struct alias *to;

	MOWGLI_PATRICIA_FOREACH(to, &state, aliases) {
		u_cmd_unreg(&to->cmd);
		free(to);
	}

	mowgli_patricia_destroy(aliases, NULL, NULL);
}

TETHYS_MODULE_V1(
	"extra/alias",
	"Alex Iadicicco <http://github.com/aji>",
	"Command aliases",

	alias_init,
	alias_deinit,

	NULL
);

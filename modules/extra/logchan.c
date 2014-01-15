/* Tethys, logchan.c -- logging channel modulue
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_chan *logchan = NULL;
static u_user *loguser = NULL;

static void logchanf(const char *fmt, ...)
{
	char buf[512];
	va_list va;

	if (!logchan)
		return;

	va_start(va, fmt);
	vsnprintf(buf, 512, fmt, va);
	va_end(va);

	if (loguser == NULL) {
		u_sendto_chan(logchan, NULL, 0, ":%S NOTICE %C :%s",
			      &me, logchan, buf);
	} else {
		u_sendto_chan(logchan, NULL, 0, ":%U PRIVMSG %C :%s",
			      loguser, logchan, buf);
	}
}

static int m_logchan(u_conn *conn, u_msg *msg)
{
	logchanf("This command currently does nothing");
}

u_cmd c_logchan[] = {
	{ "LOGCHAN", CTX_USER, m_logchan, 0 },
	{ "" },
};

static void *on_log(void *unused, void *_log)
{
	struct hook_log *log = _log;
	if (log->level > LG_VERBOSE)
		return;
	logchanf("%s", log->line);
}

static void *on_pseudoclient_create(void *unused, void *pseudo)
{
	u_chanuser *cu;

	if (loguser != NULL)
		return NULL;

	loguser = USER(pseudo);

	cu = u_chan_user_add(logchan, loguser);
	cu->flags |= CU_PFX_OP;

	u_sendto_chan(logchan, NULL, ST_USERS, ":%H JOIN %C", loguser, logchan);
	u_sendto_chan(logchan, NULL, ST_USERS, ":%S MODE %C +o %s", &me,
	              logchan, loguser->nick);

	return NULL;
}

static void *on_pseudoclient_destroy(void *unused, void *pseudo)
{
	if (loguser != pseudo)
		return NULL;

	loguser = NULL;

	return NULL;
}

static int logchan_init(u_module *m)
{
	logchan = u_chan_create("&log");
	logchan->flags |= CHAN_PERMANENT;
	u_cmds_reg(c_logchan);

	u_hook_add(HOOK_LOG, on_log, NULL);

	u_hook_add("pseudoclient:create", on_pseudoclient_create, NULL);
	u_hook_add("pseudoclient:destroy", on_pseudoclient_destroy, NULL);

	return 0;
}

static void logchan_deinit(u_module *m)
{
	if (logchan != NULL)
		u_chan_drop(logchan);
}

TETHYS_MODULE_V1(
	"extra/logchan",
	"Alex Iadicicco <http://github.com/aji>",
	"Channel for logging",

	logchan_init,
	logchan_deinit
);

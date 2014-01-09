/* ircd-micro, logchan.c -- logging channel modulue
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_chan *logchan = NULL;

static void logchanf(const char *fmt, ...)
{
	char buf[512];
	va_list va;

	if (!logchan || !ircduser)
		return;

	va_start(va, fmt);
	vsnprintf(buf, 512, fmt, va);
	va_end(va);

	u_sendto_chan(logchan, NULL, 0, ":%U PRIVMSG %C :%s",
	              ircduser, logchan, buf);
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

static void *on_conf_end(void *a, void *b)
{
	u_chanuser *cu;

	logchan = u_chan_create("&log");

	cu = u_chan_user_add(logchan, USER(ircduser));
	cu->flags |= CU_PFX_OP;

	u_hook_add(HOOK_LOG, on_log, NULL);
}

static int logchan_init(u_module *m)
{
	logchan = NULL;
	u_cmds_reg(c_logchan);
	u_hook_add(HOOK_CONF_END, on_conf_end, NULL);
	return 0;
}

static void logchan_deinit(u_module *m)
{
	if (logchan != NULL)
		u_chan_drop(logchan);
}

MICRO_MODULE_V1(
	"extra/logchan",
	"Alex Iadicicco <http://github.com/aji>",
	"Channel for logging",

	logchan_init,
	logchan_deinit
);

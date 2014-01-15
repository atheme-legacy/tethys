/* Tethys, log.c -- logging
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

int default_handler(int level, char *tm, char *line)
{
	static char *prefix[] =
		{ "!!! SEVERE: ",
		  "!!! ERROR: ",
		  "Warning: ",
		  "",
		  "",
		  "  ",
		  "    "
	};
	printf("[%s] %s%s\n", tm, prefix[level], line);
	return 0;
}

int (*u_log_handler)(int, char*, char*) = default_handler;
int u_log_level = LG_INFO;

int u_log(int level, char* fmt, ...)
{
	static bool logging = false;
	struct tm *tm;
	char tmbuf[512];
	char buf[BUFSIZE];
	va_list va;

	if (logging)
		return 0;
	logging = true;

	if (level > u_log_level)
		return 0;

	va_start(va, fmt);
	vsnf(FMT_LOG, buf, BUFSIZE, fmt, va);
	va_end(va);

	tm = localtime(&NOW.tv_sec);
	snf(FMT_LOG, tmbuf, 512, "%04d/%02d/%02d %02d:%02d:%02d",
	    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);

	struct hook_log hook;
	hook.level = level;
	hook.time = tmbuf;
	hook.line = buf;
	u_hook_call(HOOK_LOG, &hook);

	logging = false;

	return u_log_handler(level, tmbuf, buf);
}

void u_perror_real(const char *func, const char *s)
{
	int x = errno;
	char *error = strerror(x);

	u_log(LG_ERROR, "%s: %s: %s", func, s, error);
}

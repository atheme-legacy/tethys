/* ircd-micro, log.c -- logging
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
	struct tm *tm;
	char tmbuf[512];
	char buf[BUFSIZE];
	va_list va;

	if (level > u_log_level)
		return 0;

	va_start(va, fmt);
	vsnf(FMT_LOG, buf, BUFSIZE, fmt, va);
	va_end(va);

	tm = localtime(&NOW.tv_sec);
	snf(FMT_LOG, tmbuf, 512, "%04d/%02d/%02d %02d:%02d:%02d",
	    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);

	return u_log_handler(level, tmbuf, buf);
}

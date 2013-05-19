/* ircd-micro, log.c -- logging
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void default_handler(level, tm, line) char *tm, *line;
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
}

void (*u_log_handler)() = default_handler;
int u_log_level = LG_INFO;

void u_log(T(int) level, T(char*) fmt, u_va_alist) A(char *fmt; va_dcl)
{
	struct tm *tm;
	char tmbuf[512];
	char buf[BUFSIZE];
	va_list va;

	if (level > u_log_level)
		return;

	u_va_start(va, fmt);
	vsnf(FMT_LOG, buf, BUFSIZE, fmt, va);
	va_end(va);

	tm = localtime(&NOW.tv_sec);
	snf(FMT_LOG, tmbuf, 512, "%04d/%02d/%02d %02d:%02d:%02d",
	    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);

	u_log_handler(level, tmbuf, buf);
}

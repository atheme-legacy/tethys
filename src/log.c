/* ircd-micro, log.c -- logging
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void default_handler(level, line) char *line;
{
	static char *prefix[] =
		{ "=!= | SEVERE: ",
		  "=!= | ERROR: ",
		  "=!= | ",
		  " -- | ",
		  "    | ",
		  "    |   ",
		  "    |     "
	};
	printf("%s%s\n", prefix[level], line);
}

void (*u_log_handler)() = default_handler;
int u_log_level = LG_INFO;

#ifdef STDARG
void u_log(int level, char *fmt, ...)
#else
void u_log(level, fmt, va_alist) char *fmt; va_dcl
#endif
{
	char buf[BUFSIZE];
	va_list va;

	if (level > u_log_level)
		return;

	u_va_start(va, fmt);
	vsnf(FMT_LOG, buf, BUFSIZE, fmt, va);
	va_end(va);

	u_log_handler(level, buf);
}

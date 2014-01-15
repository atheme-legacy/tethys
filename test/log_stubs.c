/* tethys, log_stubs.c -- logging stubs, for debugging
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "../include/log.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

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
	struct timeval now;
	struct tm *tm;
	char tmbuf[512];
	char buf[2048];
	va_list va;

	if (level > u_log_level)
		return 0;

	va_start(va, fmt);
	vsnprintf(buf, 2048, fmt, va);
	va_end(va);

	gettimeofday(&now, NULL);
	tm = localtime(&now.tv_sec);
	snprintf(tmbuf, 512, "%04d/%02d/%02d %02d:%02d:%02d",
	    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);

	return u_log_handler(level, tmbuf, buf);
}

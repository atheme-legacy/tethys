/* ircd-micro, log.h -- logging
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_LOG_H__
#define __INC_LOG_H__

#define LG_SEVERE   0
#define LG_ERROR    1
#define LG_WARN     2
#define LG_INFO     3
#define LG_VERBOSE  4
#define LG_DEBUG    5

extern void (*u_log_handler)(); /* int level, char *line (no EOL) */
extern int u_log_level;

extern void u_log(A3(int level, char *fmt, ...));

extern int init_log();

#endif

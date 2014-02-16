/* Tethys, ircd.h -- main IRCD include file
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_IRCD_H__
#define __INC_IRCD_H__

#include "autoconf.h"

#define PACKAGE_FULLNAME PACKAGE_NAME "-" PACKAGE_VERSION
#define PACKAGE_COPYRIGHT "Copyright (C) 2013 Alex Iadicicco " \
			"and Tethys contributors"

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>

#include <mowgli.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

/* size of string buffers allocated on the stack */
#define BUFSIZE 4096

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned short ushort;

typedef unsigned long u_ts_t;

#ifndef offsetof
#define offsetof(st, m) ((ulong)(&((st *)0)->m))
#endif

#define containerof(ptr, st, m) ((void*)((ptr) - offsetof(st, m)))

#define memberp(base, offs) ((void*)((ulong)(base) + (ulong)(offs)))
#define member(mtype, base, offs) (*((mtype*)memberp(base, offs)))

#define _stringify(x) #x
#define stringify(x) _stringify(x)

#include "autoconf.h"
#include "conf.h"
#include "cookie.h"
#include "crypto.h"
#include "map.h"
#include "strop.h"
#include "upgrade.h"
#include "version.h"
#include "vsnf.h"
#include "log.h"

#include "numeric.h"

#include "auth.h"
#include "chan.h"
#include "conn.h"
#include "hook.h"
#include "link.h"
#include "mode.h"
#include "module.h"
#include "msg.h"
#include "ratelimit.h"
#include "sendto.h"
#include "server.h"
#include "user.h"
#include "util.h"

extern struct timeval NOW;

extern void sync_time(void);

extern mowgli_eventloop_t *base_ev;
extern mowgli_dns_t *base_dns;
extern u_ts_t started;
extern char startedstr[256];

extern char *crypt(const char*, const char*);

#endif

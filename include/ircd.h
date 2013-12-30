/* ircd-micro, ircd.h -- main IRCD include file
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_IRCD_H__
#define __INC_IRCD_H__

#define PACKAGE_NAME "ircd-micro"
#define PACKAGE_VERSION "0.1-dev"
#define PACKAGE_FULLNAME PACKAGE_NAME "-" PACKAGE_VERSION
#define PACKAGE_COPYRIGHT "Copyright (C) 2013 Alex Iadicicco " \
			"and ircd-micro contributors"

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

#define offsetof(st, m) ((ulong)(&((st *)0)->m))
#define containerof(ptr, st, m) ((void*)((ptr) - offsetof(st, m)))

#define memberp(base, offs) ((void*)((ulong)(base) + (ulong)(offs)))
#define member(mtype, base, offs) (*((mtype*)memberp(base, offs)))

#define _stringify(x) #x
#define stringify(x) _stringify(x)

#include "numeric.h"
#include "version.h"
#include "vsnf.h"
#include "crypto.h"
#include "log.h"
#include "list.h"
#include "map.h"
#include "trie.h"
#include "conf.h"
#include "io.h"
#include "dns.h"
#include "util.h"
#include "auth.h"
#include "conn.h"
#include "mode.h"
#include "server.h"
#include "user.h"
#include "entity.h"
#include "chan.h"
#include "msg.h"
#include "sendto.h"
#include "upgrade.h"

extern u_io base_io;
extern u_ts_t started;

extern char *crypt(const char*, const char*);

#endif

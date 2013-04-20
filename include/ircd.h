/* ircd-micro, ircd.h -- main IRCD include file
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_IRCD_H__
#define __INC_IRCD_H__

#define PACKAGE_NAME "ircd-micro"
#define PACKAGE_VERSION "0.1-dev"
#define PACKAGE_FULLNAME "ircd-micro-0.1-dev"

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

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

/* size of string buffers allocated on the stack */
#define BUFSIZE 4096

#define offsetof(st, m) ((unsigned)(&((st *)0)->m))
#define containerof(ptr, st, m) ((void*)((ptr) - offsetof(st, m)))

#define memberp(base, offs) ((void*)((int)(base) + (int)(offs)))
#define member(mtype, base, offs) (*((mtype*)memberp(base, offs)))

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned char uchar;

#ifdef __GNUC__

# undef U_BSD
# define STDARG

# include <stdarg.h>
# include <getopt.h>
# include <stdlib.h>
# include <unistd.h>
# include <sys/time.h>
# include <time.h>

# define A2(x,y) x,y
# define A3(x,y,z) x,y,z
# define A4(w,x,y,z) w,x,y,z
# define A5(v,w,x,y,z) v,w,x,y,z

# define u_va_start(va, arg) va_start(va, arg)
# define u_va_copy(a1, a2) va_copy(a1, a2)

typedef unsigned short ushort;

#else

# define U_BSD
# undef STDARG

# include <varargs.h>
# include <sys/time.h>

# define A2(x,y)
# define A3(x,y,z)
# define A4(w,x,y,z)
# define A5(v,w,x,y,z)

# define u_va_start(va, arg) va_start(va)
# define u_va_copy(a1, a2) (a1) = (a2)

extern void *malloc();

#endif

typedef unsigned long u_ts_t;

#include "util.h"
#include "log.h"
#include "list.h"
#include "trie.h"
#include "map.h"
#include "conf.h"
#include "io.h"
#include "dns.h"
#include "cookie.h"
#include "linebuf.h"
#include "numeric.h"
#include "conn.h"
#include "msg.h"
#include "server.h"
#include "user.h"
#include "chan.h"
#include "sendto.h"
#include "vsnf.h"
#include "version.h"

#endif

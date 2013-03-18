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
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>

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

#ifdef __GNUC__

# undef U_BSD
# define STDARG

# include <stdarg.h>
# include <getopt.h>
# include <stdlib.h>

# define A2(x,y) x,y
# define A3(x,y,z) x,y,z
# define u_va_start(va, arg) va_start(va, arg)

#else

# define U_BSD
# undef STDARG

# include <varargs.h>

# define A2(x,y)
# define A3(x,y,z)
# define u_va_start(va, arg) va_start(va)

extern void *malloc();

#endif

#include "util.h"
#include "log.h"
#include "list.h"
#include "trie.h"
#include "conf.h"
#include "io.h"
#include "cookie.h"
#include "linebuf.h"
#include "numeric.h"
#include "conn.h"
#include "msg.h"
#include "user.h"
#include "server.h"
#include "chan.h"

#endif

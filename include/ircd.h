/* ircd-micro, ircd.h -- main IRCD include file
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_IRCD_H__
#define __INC_IRCD_H__

#define PACKAGE_NAME "ircd-micro"
#define PACKAGE_VERSION "0.1-dev"
#define PACKAGE_FULLNAME "ircd-micro-0.1-dev"
#define PACKAGE_COPYRIGHT "Copyright (C) 2013 Alex Iadicicco and ircd-micro contributors"

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

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned char uchar;

#define offsetof(st, m) ((ulong)(&((st *)0)->m))
#define containerof(ptr, st, m) ((void*)((ptr) - offsetof(st, m)))

#define memberp(base, offs) ((void*)((ulong)(base) + (ulong)(offs)))
#define member(mtype, base, offs) (*((mtype*)memberp(base, offs)))

#ifdef __GNUC__

# undef U_BSD
# define STDARG

# include <stdarg.h>
# include <getopt.h>
# include <stdlib.h>
# include <unistd.h>
# include <sys/time.h>
# include <time.h>

# ifndef __APPLE__
#  include <crypt.h>
# endif

# define A(x)
# define A2(x,y) x,y
# define A3(x,y,z) x,y,z
# define A4(w,x,y,z) w,x,y,z
# define A5(v,w,x,y,z) v,w,x,y,z
# define T(x) x

# define u_va_alist ...
# define u_va_start(va, arg) va_start(va, arg)
# define u_va_copy(a1, a2) va_copy(a1, a2)

# define _stringify(x) #x
# define stringify(x) _stringify(x)

typedef unsigned short ushort;

#else

# define U_BSD
# undef STDARG

# include <varargs.h>
# include <sys/time.h>

# define A(x) x
# define A2(x,y)
# define A3(x,y,z)
# define A4(w,x,y,z)
# define A5(v,w,x,y,z)
# define T(x)

# define u_va_alist va_alist
# define u_va_start(va, arg) va_start(va)
# define u_va_copy(a1, a2) (a1) = (a2)

# define _stringify(x) "x"
# define stringify(x) _stringify(x)

extern void *malloc();

#endif

typedef unsigned long u_ts_t;

#include "util.h"
#include "crypto.h"
#include "log.h"
#include "list.h"
#include "trie.h"
#include "map.h"
#include "upgrade.h"
#include "conf.h"
#include "auth.h"
#include "io.h"
#include "dns.h"
#include "cookie.h"
#include "linebuf.h"
#include "mode.h"
#include "numeric.h"
#include "conn.h"
#include "server.h"
#include "user.h"
#include "chan.h"
#include "entity.h"
#include "msg.h"
#include "sendto.h"
#include "vsnf.h"
#include "version.h"

extern u_io base_io;
extern u_ts_t started;

#endif

#ifndef __INC_IRCD_H__
#define __INC_IRCD_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <getopt.h>
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

#ifdef __GNUC__
# undef U_BSD
# define STDARG
# include <stdarg.h>
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
#include "linebuf.h"
#include "numeric.h"
#include "conn.h"
#include "msg.h"
#include "user.h"
#include "server.h"

#endif

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

#ifndef NULL
#define NULL ((void*)0)
#endif

#define offsetof(st, m) ((unsigned)(&((st *)0)->m))
#define containerof(ptr, st, m) ((void*)((ptr) - offsetof(st, m)))

#ifdef DEBUG
#define u_debug(x...) printf(x)
#define u_log(x...) printf(x)
#else
#define u_debug(x...)
#define u_log(x...)
#endif

#ifdef __GNUC__
# define STDARG
# include <stdarg.h>
# define A(x...) x
#else
# undef STDARG
# include <varargs.h>
# define A(x...)
#endif

#include "util.h"
#include "heap.h"
#include "list.h"
#include "trie.h"
#include "io.h"
#include "linebuf.h"
#include "conn.h"
#include "msg.h"
#include "user.h"
#include "server.h"
#include "toplev.h"

#endif

#ifndef __INC_IRCD_H__
#define __INC_IRCD_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
//#include <varargs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define offsetof(st, m) ((unsigned)(&((st *)0)->m))
#define containerof(ptr, st, m) ((void*)((ptr) - offsetof(st, m)))

#include "hash.h"
#include "list.h"
#include "io.h"
#include "linebuf.h"
#include "connection.h"
#include "msg.h"
#include "user.h"
#include "server.h"

#endif

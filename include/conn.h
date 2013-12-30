/* ircd-micro, conn.h -- line-based connection management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_CONN_H__
#define __INC_CONN_H__

#include "ircd.h"
#include "linebuf.h"
#include "cookie.h"

#define U_CONN_OBUFSIZE 32768
#define U_CONN_HOSTSIZE 256

#define U_CONN_CLOSING    0x0001
#define U_CONN_AWAIT_PONG 0x0002

/* events */
#define EV_ERROR          1
#define EV_DESTROYING     2

/* connection contexts. used for command processing */
#define CTX_UNREG       0
#define CTX_USER        1
#define CTX_SERVER      2
#define CTX_UREG        3
#define CTX_SREG        4
#define CTX_CLOSED      5
#define CTX_MAX         6

typedef struct u_conn u_conn;
typedef struct u_conn_origin u_conn_origin;

#include "auth.h"

struct u_conn {
	uint flags;
	int ctx;
	u_auth *auth;
	u_ts_t last;

	u_io_fd *sock;
	char ip[INET_ADDRSTRLEN];
	char host[U_CONN_HOSTSIZE];
	ushort dns_id;

	u_linebuf ibuf;

	char *obuf;
	int obuflen, obufsize;
	u_cookie ck_sendto;

	void (*event)(u_conn*, int event);
	char *error;

	void *priv;
	char *pass;
};

struct u_conn_origin {
	u_io_fd *sock;
};

extern void u_conn_init(u_conn*);
extern void u_conn_close(u_conn*);
extern void u_conn_obufsize(u_conn*, int obufsize);

extern u_conn *u_conn_by_name(char *nick_or_server);

extern void u_conn_vf(u_conn*, char *fmt, va_list);
extern void u_conn_f(u_conn *conn, char *fmt, ...);

extern void u_conn_vnum(u_conn*, char *tgt, int num, va_list);
extern int u_conn_num(u_conn *conn, int num, ...);

extern void u_conn_error(u_conn*, char*);

extern u_conn_origin *u_conn_origin_create(u_io*, ulong addr, ushort port);

#endif

/* Tethys, conn.h -- line-based connection management
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
#define U_CONN_SHUTDOWN   0x0004
#define U_CONN_DESTROY    0x0008
#define U_CONN_REGISTERED 0x0010
#define U_CONN_NO_SEND    0x0001 /* !!!: same as U_CONN_CLOSING */

/* connection contexts. describes nature of conn->priv */
#define CTX_NONE        0
#define CTX_USER        1
#define CTX_SERVER      2

typedef struct u_conn u_conn;
typedef struct u_conn_origin u_conn_origin;

#include "auth.h"

struct u_conn {
	mowgli_node_t n;

	uint flags;
	int ctx;
	void *priv;

	char *pass;
	u_auth *auth;

	mowgli_eventloop_pollable_t *poll;
	char ip[INET6_ADDRSTRLEN];
	char host[U_CONN_HOSTSIZE];
	mowgli_dns_query_t *dnsq;

	u_linebuf ibuf;
	u_ts_t last;

	char *obuf;
	int obuflen, obufsize;
	u_cookie ck_sendto;

	void (*shutdown)(u_conn*);
	char *error;
};

struct u_conn_origin {
	mowgli_eventloop_pollable_t *poll;
	mowgli_node_t n;
};

extern u_conn *u_conn_create(mowgli_eventloop_t *ev, int fd);
extern void u_conn_destroy(u_conn*);

extern void u_conn_shutdown(u_conn*);
extern void u_conn_fatal(u_conn*, char *reason);

extern void u_conn_obufsize(u_conn*, int obufsize);

extern u_conn *u_conn_by_name(char *nick_or_server);

extern void u_conn_vf(u_conn*, char *fmt, va_list);
extern void u_conn_f(u_conn *conn, char *fmt, ...);

extern void u_conn_vnum(u_conn*, char *tgt, int num, va_list);
extern int u_conn_num(u_conn *conn, int num, ...);

extern u_conn_origin *u_conn_origin_create(mowgli_eventloop_t*, ulong addr,
                                           ushort port);
extern void u_conn_origin_destroy(u_conn_origin *orig);

extern void u_conn_check_ping_all(void*);

extern void u_conn_run(mowgli_eventloop_t *ev);

extern int init_conn(void);

#endif

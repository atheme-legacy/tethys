/* Tethys, link.h -- IRC connections
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_LINK_H__
#define __INC_LINK_H__

typedef struct u_link u_link;
typedef enum u_link_type u_link_type;

#include "conn.h"

enum u_link_type {
	LINK_NONE,
	LINK_USER,
	LINK_SERVER,
};

/* of these, only RDNS is currently used */
#define U_LINK_WAIT_RDNS         0x0001
#define U_LINK_WAIT_IDENTD       0x0002
#define U_LINK_WAIT_PING_COOKIE  0x0004
#define U_LINK_WAIT_FLOOD        0x0008
#define U_LINK_WAIT              0x000f

#define U_LINK_SENT_QUIT         0x0010
#define U_LINK_REGISTERED        0x0020

#define IBUFSIZE 2048

struct u_link {
	u_conn *conn;

	uint flags;
	u_link_type type;
	void *priv;

	char *pass;
	u_auth_block *auth;

	uchar ibuf[IBUFSIZE+1];
	size_t ibuflen;

	u_cookie ck_sendto;
};

extern u_conn_ctx u_link_conn_ctx;

extern void u_link_attach(u_conn *conn);
extern void u_link_detach(u_conn *conn);

extern void u_link_fatal(u_link *link, const char *msg);

extern void u_link_vf(u_link *link, const char *fmt, va_list va);
extern void u_link_f(u_link *link, const char *fmt, ...);

extern void u_link_vnum(u_link *link, const char *tgt, int num, va_list va);
extern int u_link_num(u_link *link, int num, ...);

#endif

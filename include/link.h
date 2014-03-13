/* Tethys, link.h -- IRC connections
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_LINK_H__
#define __INC_LINK_H__

typedef enum u_link_type u_link_type;
typedef struct u_link u_link;
typedef struct u_link_origin u_link_origin;

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
#define U_LINK_SENT_PASS         0x0040

#define IBUFSIZE 2048

struct u_link {
	u_conn *conn;

	uint flags;
	u_link_type type;
	void *priv;

	char *pass;
	union {
		u_auth_block *auth;
		u_link_block *link;
	} conf;
	int sendq;

	uchar ibuf[IBUFSIZE+1];
	size_t ibuflen;

	/* This indicates that X bytes should be skipped in ibuf when serializing
	 * ibuf to do an upgrade. This is necessary to prevent the UPGRADE command
	 * itself from re-executing after the upgrade.
	 */
	size_t ibufskip;

	u_cookie ck_sendto;
};

extern u_conn_ctx u_link_conn_ctx;

extern u_link *u_link_connect(mowgli_eventloop_t*, u_link_block*,
                              const struct sockaddr*, socklen_t);
extern void u_link_close(u_link *link);
extern void u_link_fatal(u_link *link, const char *msg);

extern void u_link_vf(u_link *link, const char *fmt, va_list va);
extern void u_link_f(u_link *link, const char *fmt, ...);

extern void u_link_vnum(u_link *link, const char *tgt, int num, va_list va);
extern int u_link_num(u_link *link, int num, ...);
extern void u_link_flush_input(u_link *link);

extern u_link_origin *u_link_origin_create(mowgli_eventloop_t*, short);

extern int init_link(void);

extern mowgli_json_t *u_link_to_json(u_link *link);
extern u_link *u_link_from_json(mowgli_json_t *j);

#endif

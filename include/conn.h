/* ircd-micro, conn.h -- line-based connection management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_CONN_H__
#define __INC_CONN_H__

#define U_CONN_OBUFSIZE 32768
#define U_CONN_HOSTSIZE 256

#define U_CONN_CLOSING 0x0001

/* events */
#define EV_ERROR          1
#define EV_DESTROYING     2

/* connection contexts. used for command processing */
#define CTX_UNREG       0
#define CTX_USER        1
#define CTX_SERVER      2
#define CTX_UREG        3
#define CTX_SREG        4
#define CTX_MAX         5

struct u_conn {
	unsigned flags;
	int ctx;

	struct u_io_fd *sock;
	char ip[INET_ADDRSTRLEN];
	char host[U_CONN_HOSTSIZE];
	unsigned short dns_id;

	struct u_linebuf ibuf;

	char *obuf;
	int obuflen, obufsize;
	struct u_cookie ck_sendto;

	void (*event)(); /* u_conn*, int event */
	char *error;

	void *priv;
	char *pass;
};

struct u_conn_origin {
	struct u_io_fd *sock;
};

extern void u_conn_init(); /* u_conn* */
extern void u_conn_cleanup(); /* u_conn* */
extern void u_conn_obufsize(); /* u_conn*, int obufsize */

extern void u_conn_vf(); /* u_conn*, char *fmt, va_list */
extern void u_conn_f(A3(struct u_conn *conn, char *fmt, ...));

extern void u_conn_vnum(); /* u_conn*, char *nick, int num, va_list */
extern void u_conn_num(A3(struct u_conn *conn, int num, ...));

extern void u_conn_error(); /* u_conn*, char* */

/* u_io*, u_long addr, u_short port */
extern struct u_conn_origin *u_conn_origin_create();

#endif

/* ircd-micro, sendto.c -- unified message sending
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_cookie ck_sendto;

static void start()
{
	u_cookie_inc(&ck_sendto);
}

static void exclude(conn) u_conn *conn;
{
	if (u_cookie_cmp(&conn->ck_sendto, &ck_sendto) < 0)
		u_cookie_cpy(&conn->ck_sendto, &ck_sendto);
}

static void sendto_chan_cb(map, u, cu, buf)
u_map *map; u_user *u; u_chanuser *cu; char *buf;
{
	u_conn *conn = u_user_conn(u);

	if (!u_cookie_cmp(&conn->ck_sendto, &ck_sendto))
		return;

	u_conn_f(conn, "%s", buf);
	u_cookie_cpy(&conn->ck_sendto, &ck_sendto);
}

#ifdef STDARG
void u_sendto_chan(u_chan *c, u_conn *conn, char *fmt, ...)
#else
void u_sendto_chan(c, conn, fmt, va_alist)
u_chan *c; u_conn *conn; char *fmt; va_dcl
#endif
{
	char buf[4096];
	va_list va;

	start();

	if (conn != NULL)
		exclude(conn);

	u_va_start(va, fmt);
	vsprintf(buf, fmt, va);
	va_end(va);

	u_map_each(c->members, sendto_chan_cb, buf);
}

static void sendto_visible_cb(map, c, cu, buf)
u_map *map; u_chan *c; u_chanuser *cu; char *buf;
{
	u_map_each(c->members, sendto_chan_cb, buf);
}

#ifdef STDARG
void u_sendto_visible(u_user *u, char *fmt, ...)
#else
void u_sendto_visible(u, fmt, va_alist)
u_user *u; char *fmt; va_dcl
#endif
{
	char buf[4096];
	va_list va;

	start();
	exclude(u_user_conn(u));

	u_va_start(va, fmt);
	vsprintf(buf, fmt, va);
	va_end(va);

	u_map_each(u->channels, sendto_visible_cb, buf);
}

int init_sendto()
{
	u_cookie_reset(&ck_sendto);
	return 0;
}

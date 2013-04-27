/* ircd-micro, sendto.c -- unified message sending
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_cookie ck_sendto;

char buf_serv[1024];
char buf_user[1024];

struct sendto_priv {
	char *serv, *user;
	char *fmt;
	va_list va;
};

static void start()
{
	u_cookie_inc(&ck_sendto);
}

static void exclude(conn) u_conn *conn;
{
	if (u_cookie_cmp(&conn->ck_sendto, &ck_sendto) < 0)
		u_cookie_cpy(&conn->ck_sendto, &ck_sendto);
}

static char *ln(pv, conn) struct sendto_priv *pv; u_conn *conn;
{
	va_list va;

	switch (conn->ctx) {
	case CTX_USER:
	case CTX_UNREG:
	case CTX_UREG:
		if (!pv->user) {
			pv->user = buf_user;
			u_va_copy(va, pv->va);
			vsnf(FMT_USER, buf_user, 1024, pv->fmt, va);
			va_end(va);
		}
		return pv->user;

	case CTX_SERVER:
	case CTX_SREG:
		if (!pv->serv) {
			pv->serv = buf_serv;
			u_va_copy(va, pv->va);
			vsnf(FMT_SERVER, buf_serv, 1024, pv->fmt, va);
			va_end(va);
		}
		return pv->serv;
	}

	return NULL;
}

static void sendto_chan_cb(map, u, cu, priv)
u_map *map; u_user *u; u_chanuser *cu; struct sendto_priv *priv;
{
	u_conn *conn = u_user_conn(u);
	char *s;

	if (!u_cookie_cmp(&conn->ck_sendto, &ck_sendto))
		return;

	s = ln(priv, conn);
	u_conn_f(conn, "%s", s);
	u_cookie_cpy(&conn->ck_sendto, &ck_sendto);
}

#ifdef STDARG
void u_sendto_chan(u_chan *c, u_conn *conn, char *fmt, ...)
#else
void u_sendto_chan(c, conn, fmt, va_alist)
u_chan *c; u_conn *conn; char *fmt; va_dcl
#endif
{
	struct sendto_priv priv;

	start();

	if (conn != NULL)
		exclude(conn);

	priv.user = priv.serv = NULL;
	priv.fmt = fmt;
	u_va_start(priv.va, fmt);
	u_map_each(c->members, sendto_chan_cb, &priv);
	va_end(priv.va);
}

static void sendto_visible_cb(map, c, cu, priv)
u_map *map; u_chan *c; u_chanuser *cu; struct sendto_priv *priv;
{
	u_map_each(c->members, sendto_chan_cb, priv);
}

#ifdef STDARG
void u_sendto_visible(u_user *u, char *fmt, ...)
#else
void u_sendto_visible(u, fmt, va_alist)
u_user *u; char *fmt; va_dcl
#endif
{
	struct sendto_priv priv;

	start();

	exclude(u_user_conn(u));

	priv.user = priv.serv = NULL;
	priv.fmt = fmt;
	u_va_start(priv.va, fmt);
	u_map_each(u->channels, sendto_visible_cb, &priv);
	va_end(priv.va);
}

int init_sendto()
{
	u_cookie_reset(&ck_sendto);
	return 0;
}

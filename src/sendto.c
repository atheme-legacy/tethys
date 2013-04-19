/* ircd-micro, sendto.c -- unified message sending
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

/* XXX: a rather nasty implication of the way things work now with vsnf()
   is that vsnf() is called for each matching connection, which could end
   up being a lot in extreme cases. That's a lot of duplicated work. Maybe
   sendto should be connection-aware? */

#include "ircd.h"

static u_cookie ck_sendto;

struct sendto_priv {
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

static void sendto_chan_cb(map, u, cu, priv)
u_map *map; u_user *u; u_chanuser *cu; struct sendto_priv *priv;
{
	va_list va;
	u_conn *conn = u_user_conn(u);

	if (!u_cookie_cmp(&conn->ck_sendto, &ck_sendto))
		return;

	u_va_copy(va, priv->va);
	u_conn_vf(conn, priv->fmt, va);
	va_end(va);

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

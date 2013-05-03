/* ircd-micro, sendto.c -- unified message sending
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_cookie ck_sendto;

static u_map *rosters[256];

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

void u_sendto_chan(T(u_chan*) c, T(u_conn*) conn, T(char*) fmt, u_va_alist)
A(u_chan *c; u_conn *conn; char *fmt; va_dcl)
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

void u_sendto_visible(T(u_user*) u, T(char*) fmt, u_va_alist)
A(u_user *u; char *fmt; va_dcl)
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

static char *roster_to_str(c) unsigned char c;
{
	static char buf[16];
	char *high = (c / 0x80) ? "+0x80" : "";

	c &= 0x7f;
	if (c < 0x20)
		snf(FMT_LOG, buf, 16, "\\%d%s", c, high);
	else
		snf(FMT_LOG, buf, 16, "'%c'%s", c, high);

	return buf;
}

void u_roster_add(r, ul) unsigned char r; u_user_local *ul;
{
	if (!r) return;
	u_log(LG_DEBUG, "Adding %U to roster %s", USER(ul), roster_to_str(r));
	u_map_set(rosters[r], ul, ul->conn);
}

void u_roster_del(r, ul) unsigned char r; u_user_local *ul;
{
	if (!r) return;
	u_log(LG_DEBUG, "Removing %U from roster %s", USER(ul), roster_to_str(r));
	u_map_del(rosters[r], ul);
}

void u_roster_del_all(ul) u_user_local *ul;
{
	unsigned int c;
	u_log(LG_DEBUG, "Removing %U from all rosters", USER(ul));
	for (c=1; c<256; c++)
		u_map_del(rosters[c], ul);
}

void roster_f_cb(map, ul, conn, priv)
u_map *map; u_user_local *ul; u_conn *conn; struct sendto_priv *priv;
{
	char *s;

	if (!u_cookie_cmp(&conn->ck_sendto, &ck_sendto))
		return;

	s = ln(priv, conn);
	u_conn_f(conn, "%s", s);
	u_cookie_cpy(&conn->ck_sendto, &ck_sendto);
}

void u_roster_f(T(unsigned char) c, T(char*) fmt, u_va_alist)
A(unsigned char c; char **fmt; va_dcl)
{
	struct sendto_priv priv;

	start();

	u_log(LG_DEBUG, "Sending '%s' to roster %s", fmt, roster_to_str(c));

	priv.user = priv.serv = NULL;
	priv.fmt = fmt;
	u_va_start(priv.va, fmt);
	u_map_each(rosters[(unsigned)c], roster_f_cb, &priv);
	va_end(priv.va);
}

void u_wallops(T(char*) fmt, u_va_alist) A(char *fmt; va_dcl)
{
	char buf[512];
	va_list va;

	u_va_start(va, fmt);
	vsnf(FMT_USER, buf, 512, fmt, va);
	va_end(va);

	u_roster_f(R_WALLOPS, ":%S WALLOPS :%s", &me, buf);
}

int init_sendto()
{
	int i;

	for (i=0; i<256; i++)
		rosters[i] = u_map_new(0);

	u_cookie_reset(&ck_sendto);

	return 0;
}

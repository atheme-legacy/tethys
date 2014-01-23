/* Tethys, sendto.c -- unified message sending
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
	uint type;
	va_list va;
};

void u_st_start(void)
{
	u_cookie_inc(&ck_sendto);
}

void u_st_exclude(u_conn *conn)
{
	if (!conn)
		return;

	if (u_cookie_cmp(&conn->ck_sendto, &ck_sendto) < 0)
		u_cookie_cpy(&conn->ck_sendto, &ck_sendto);
}

int u_st_match_server(u_st_opts *opt, u_server *sv)
{
	if (opt->type == ST_USERS)
		return 0;
	return 1;
}

int u_st_match_user(u_st_opts *opt, u_user *u)
{
	if (opt->type == ST_SERVERS)
		return 0;

	if ((u->flags & opt->flags_all) != opt->flags_all)
		return 0;
	if (u->flags & opt->flags_none)
		return 0;

	if (opt->c) {
		u_chanuser *cu = u_chan_user_find(opt->c, u);
		if (!cu || (cu->flags & opt->cu_flags) != opt->cu_flags)
			return 0;
	}

	return 1;
}

int u_st_match_conn(u_st_opts *opt, u_conn *conn)
{
	if (conn->ctx == CTX_SERVER)
		return u_st_match_server(opt, conn->priv);
	return u_st_match_user(opt, conn->priv);
}

static int want_send(struct sendto_priv *pv, u_conn *conn)
{
	switch (pv->type) {
	case ST_ALL:
		return 1;

	case ST_USERS:
	case ST_SERVERS:
		switch (conn->ctx) {
		case CTX_NONE:
		case CTX_USER:
			return pv->type == ST_USERS;

		case CTX_SERVER:
			return pv->type == ST_SERVERS;
		}
		return 0;
	}

	return 0;
}

static char *ln(struct sendto_priv *pv, u_conn *conn)
{
	va_list va;

	switch (conn->ctx) {
	case CTX_NONE:
	case CTX_USER:
		if (!pv->user) {
			pv->user = buf_user;
			va_copy(va, pv->va);
			vsnf(FMT_USER, buf_user, 1024, pv->fmt, va);
			va_end(va);
		}
		return pv->user;

	case CTX_SERVER:
		if (!pv->serv) {
			pv->serv = buf_serv;
			va_copy(va, pv->va);
			vsnf(FMT_SERVER, buf_serv, 1024, pv->fmt, va);
			va_end(va);
		}
		return pv->serv;
	}

	return NULL;
}

static void sendto_chan_cb(u_map *map, u_user *u, u_chanuser *cu,
                           struct sendto_priv *priv)
{
	u_conn *conn = u_user_conn(u);
	char *s;

	if (!conn)
		return;

	if (!u_cookie_cmp(&conn->ck_sendto, &ck_sendto))
		return;

	if (want_send(priv, conn)) {
		u_cookie_cpy(&conn->ck_sendto, &ck_sendto);
		s = ln(priv, conn);
		u_conn_f(conn, "%s", s);
	}
}

void u_sendto_chan(u_chan *c, u_conn *conn, uint type, char *fmt, ...)
{
	struct sendto_priv priv;

	u_st_start();

	if (conn != NULL)
		u_st_exclude(conn);

	priv.user = priv.serv = NULL;
	priv.fmt = fmt;
	priv.type = type;
	va_start(priv.va, fmt);
	u_map_each(c->members, (u_map_cb_t*)sendto_chan_cb, &priv);
	va_end(priv.va);
}

static void sendto_visible_cb(u_map *map, u_chan *c, u_chanuser *cu,
                              struct sendto_priv *priv)
{
	u_map_each(c->members, (u_map_cb_t*)sendto_chan_cb, priv);
}

void u_sendto_visible(u_user* u, uint type, char* fmt, ...)
{
	struct sendto_priv priv;

	u_st_start();

	u_st_exclude(u_user_conn(u));

	priv.user = priv.serv = NULL;
	priv.fmt = fmt;
	priv.type = type;
	va_start(priv.va, fmt);
	u_map_each(u->channels, (u_map_cb_t*)sendto_visible_cb, &priv);
	va_end(priv.va);
}

static char *roster_to_str(uchar c)
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

void u_roster_add(uchar r, u_conn *conn)
{
	if (!r || !conn) return;
	u_log(LG_DEBUG, "Adding %G to roster %s", conn, roster_to_str(r));
	u_map_set(rosters[r], conn, conn);
}

void u_roster_del(uchar r, u_conn *conn)
{
	if (!r || !conn) return;
	u_log(LG_DEBUG, "Removing %G from roster %s", conn, roster_to_str(r));
	u_map_del(rosters[r], conn);
}

void u_roster_del_all(u_conn *conn)
{
	uint c;
	if (!conn) return;
	u_log(LG_DEBUG, "Removing %G from all rosters", conn);
	for (c=1; c<256; c++)
		u_map_del(rosters[c], conn);
}

void roster_f_cb(u_map *map, u_conn *unused, u_conn *conn, struct sendto_priv *priv)
{
	char *s;

	if (!u_cookie_cmp(&conn->ck_sendto, &ck_sendto))
		return;

	if (want_send(priv, conn)) {
		u_cookie_cpy(&conn->ck_sendto, &ck_sendto);
		s = ln(priv, conn);
		u_conn_f(conn, "%s", s);
	}
}

void u_roster_f(uchar c, u_conn *conn, char *fmt, ...)
{
	struct sendto_priv priv;

	u_st_start();

	if (conn != NULL)
		u_st_exclude(conn);

	priv.user = priv.serv = NULL;
	priv.fmt = fmt;
	priv.type = ST_ALL;

	u_log(LG_DEBUG, "roster[%s]: %s", roster_to_str(c), fmt);

	va_start(priv.va, fmt);
	u_map_each(rosters[(ulong)c], (u_map_cb_t*)roster_f_cb, &priv);
	va_end(priv.va);
}

void u_wallops(char *fmt, ...)
{
	char buf[512];
	va_list va;

	va_start(va, fmt);
	vsnf(FMT_USER, buf, 512, fmt, va);
	va_end(va);

	u_roster_f(R_WALLOPS, NULL, ":%S WALLOPS :%s", &me, buf);
}

int init_sendto(void)
{
	int i;

	for (i=0; i<256; i++)
		rosters[i] = u_map_new(0);

	u_cookie_reset(&ck_sendto);

	return 0;
}

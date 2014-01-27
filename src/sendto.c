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

static int want_send(uint type, u_conn *conn)
{
	switch (type) {
	case ST_ALL:
		return 1;

	case ST_USERS:
	case ST_SERVERS:
		switch (conn->ctx) {
		case CTX_NONE:
		case CTX_USER:
			return type == ST_USERS;

		case CTX_SERVER:
			return type == ST_SERVERS;
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

	if (want_send(priv->type, conn)) {
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

void u_sendto_chan_start(u_sendto_state *state, u_chan *c,
                         u_conn *exclude, uint type)
{
	u_st_start();

	if (exclude != NULL)
		u_st_exclude(exclude);

	state->type = type;

	u_map_each_start(&state->members, c->members);
}

bool u_sendto_chan_next(u_sendto_state *state, u_conn **conn_ret)
{
	u_conn *conn;
	u_user *u;
	u_chanuser *cu;

	conn = NULL;
next_conn:
	if (!u_map_each_next(&state->members, (void**)&u, (void**)&cu))
		return false;

	if (!(conn = u_user_conn(u)))
		goto next_conn;
	if (!u_cookie_cmp(&conn->ck_sendto, &ck_sendto))
		goto next_conn;

	*conn_ret = conn;
	return true;
}

void u_sendto_visible_start(u_sendto_state *state, u_user *u,
                            u_conn *exclude, uint type)
{
	u_st_start();

	if (exclude != NULL)
		u_st_exclude(exclude);

	state->type = type;

	u_map_each_start(&state->chans, u->channels);
	state->c = NULL;
}

bool u_sendto_visible_next(u_sendto_state *state, u_conn **conn_ret)
{
	u_conn *conn;
	u_user *u;
	u_chanuser *cu;

	conn = NULL;
next_conn:
	if (!state->c) {
		if (!u_map_each_next(&state->chans, (void**)&state->c, NULL))
			return false;
		if (!state->c)
			return false;
		u_map_each_start(&state->members, state->c->members);
	}

	if (!u_map_each_next(&state->members, (void**)&u, (void**)&cu)) {
		state->c = NULL;
		goto next_conn;
	}

	if (!(conn = u_user_conn(u)))
		goto next_conn;
	if (!u_cookie_cmp(&conn->ck_sendto, &ck_sendto))
		goto next_conn;
	if (!want_send(state->type, conn))
		goto next_conn;

	*conn_ret = conn;
	return true;
}

void u_sendto_servers_start(u_sendto_state *state, u_conn *exclude)
{
	u_st_start();

	if (exclude != NULL)
		u_st_exclude(exclude);

	mowgli_patricia_foreach_start(servers_by_sid, &state->pstate);
}

bool u_sendto_servers_next(u_sendto_state *state, u_conn **conn_ret)
{
	u_server *sv;

next_conn:
	sv = mowgli_patricia_foreach_cur(servers_by_sid, &state->pstate);
	if (sv == NULL)
		return false;
	mowgli_patricia_foreach_next(servers_by_sid, &state->pstate);

	if (!IS_SERVER_LOCAL(sv) || !sv->conn)
		goto next_conn;
	if (!u_cookie_cmp(&sv->conn->ck_sendto, &ck_sendto))
		goto next_conn;

	*conn_ret = sv->conn;
	return true;
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

	if (want_send(priv->type, conn)) {
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

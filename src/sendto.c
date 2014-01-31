/* Tethys, sendto.c -- unified message sending
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_cookie ck_sendto;

static char *ln_user, buf_user[1024];
static char *ln_serv, buf_serv[1024];

void u_sendto_start(void)
{
	u_cookie_inc(&ck_sendto);
	ln_user = NULL;
	ln_serv = NULL;
}

void u_sendto_skip(u_conn *conn)
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

static char *ln(u_conn *conn, char *fmt, va_list va_orig)
{
	va_list va;

	switch (conn->ctx) {
	case CTX_NONE:
	case CTX_USER:
		if (ln_user != NULL)
			return ln_user;
		ln_user = buf_user;
		va_copy(va, va_orig);
		vsnf(FMT_USER, buf_user, 1024, fmt, va);
		va_end(va);
		return ln_user;

	case CTX_SERVER:
		if (ln_serv != NULL)
			return ln_serv;
		ln_serv = buf_serv;
		va_copy(va, va_orig);
		vsnf(FMT_SERVER, buf_serv, 1024, fmt, va);
		va_end(va);
		return ln_serv;
	}

	return NULL;
}

void u_sendto(u_conn *conn, char *fmt, ...)
{
	va_list va;
	if (!u_cookie_cmp(&conn->ck_sendto, &ck_sendto))
		return;
	u_cookie_cpy(&conn->ck_sendto, &ck_sendto);
	va_start(va, fmt);
	u_conn_vf(conn, fmt, va);
	va_end(va);
}

void u_sendto_chan(u_chan *c, u_conn *exclude, uint type, char *fmt, ...)
{
	u_sendto_state st;
	u_conn *conn;
	va_list va;

	va_start(va, fmt);
	U_SENDTO_CHAN(&st, c, exclude, type, &conn)
		u_sendto(conn, "%s", ln(conn, fmt, va));
	va_end(va);
}

void u_sendto_visible(u_user *u, uint type, char *fmt, ...)
{
	u_sendto_state st;
	u_conn *conn;
	u_conn *exclude = u_user_conn(u);
	va_list va;

	va_start(va, fmt);
	U_SENDTO_VISIBLE(&st, u, exclude, type, &conn)
		u_sendto(conn, "%s", ln(conn, fmt, va));
	va_end(va);
}

void u_sendto_servers(u_conn *exclude, char *fmt, ...)
{
	u_sendto_state st;
	u_conn *conn;
	va_list va;

	va_start(va, fmt);
	U_SENDTO_SERVERS(&st, exclude, &conn)
		u_sendto(conn, "%s", ln(conn, fmt, va));
	va_end(va);
}

void u_sendto_list(mowgli_list_t *list, u_conn *exclude, char *fmt, ...)
{
	mowgli_node_t *n;
	va_list va;

	u_sendto_start();
	if (exclude != NULL)
		u_sendto_skip(exclude);

	va_start(va, fmt);
	MOWGLI_LIST_FOREACH(n, list->head) {
		u_conn *conn = n->data;
		u_sendto(conn, "%s", ln(conn, fmt, va));
	}
	va_end(va);
}

void u_sendto_map(u_map *map, u_conn *exclude, char *fmt, ...)
{
	u_map_each_state state;
	u_conn *conn;
	va_list va;

	u_sendto_start();
	if (exclude != NULL)
		u_sendto_skip(exclude);

	va_start(va, fmt);
	U_MAP_EACH(&state, map, NULL, &conn)
		u_sendto(conn, "%s", ln(conn, fmt, va));
	va_end(va);
}

void u_sendto_chan_start(u_sendto_state *state, u_chan *c,
                         u_conn *exclude, uint type)
{
	if (c == NULL) {
		state->type = ST_STOP;
		return;
	}

	u_sendto_start();

	if (exclude != NULL)
		u_sendto_skip(exclude);

	state->type = type;

	u_map_each_start(&state->members, c->members);
}

bool u_sendto_chan_next(u_sendto_state *state, u_conn **conn_ret)
{
	u_conn *conn;
	u_user *u;
	u_chanuser *cu;

	if (state->type == ST_STOP)
		return false;

	conn = NULL;
next_conn:
	if (!u_map_each_next(&state->members, (void**)&u, (void**)&cu))
		return false;

	if (!(conn = u_user_conn(u)))
		goto next_conn;
	if (!want_send(state->type, conn))
		goto next_conn;
	if (!u_cookie_cmp(&conn->ck_sendto, &ck_sendto))
		goto next_conn;

	*conn_ret = conn;
	return true;
}

void u_sendto_visible_start(u_sendto_state *state, u_user *u,
                            u_conn *exclude, uint type)
{
	if (u == NULL) {
		state->type = ST_STOP;
		return;
	}

	u_sendto_start();

	if (exclude != NULL)
		u_sendto_skip(exclude);

	state->type = type;

	u_map_each_start(&state->chans, u->channels);
	state->c = NULL;
}

bool u_sendto_visible_next(u_sendto_state *state, u_conn **conn_ret)
{
	u_conn *conn;
	u_user *u;
	u_chanuser *cu;

	if (state->type == ST_STOP)
		return false;

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
	u_sendto_start();

	if (exclude != NULL)
		u_sendto_skip(exclude);

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

int init_sendto(void)
{
	u_cookie_reset(&ck_sendto);

	return 0;
}

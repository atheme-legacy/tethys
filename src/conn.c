/* Tethys, conn.c -- line-based connection management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static mowgli_list_t all_conns;

static mowgli_list_t need_destroy;

static void call_shutdown(u_conn*);

u_conn *u_conn_create(mowgli_eventloop_t *ev, int fd)
{
	u_conn *conn;

	conn = malloc(sizeof(*conn));

	conn->flags = 0;
	conn->ctx = CTX_NONE;
	conn->priv = NULL;

	conn->pass = NULL;
	conn->auth = NULL;

	conn->poll = mowgli_pollable_create(ev, fd, conn);
	mowgli_pollable_set_nonblocking(conn->poll, true);

	conn->ip[0] = '\0';
	conn->host[0] = '\0';
	conn->dnsq = NULL;

	u_linebuf_init(&conn->ibuf);
	conn->last = NOW.tv_sec;

	conn->obuf = malloc(U_CONN_OBUFSIZE);
	conn->obuflen = 0;
	conn->obufsize = U_CONN_OBUFSIZE;
	u_cookie_reset(&conn->ck_sendto);

	if (conn->obuf == NULL)
		abort();

	conn->sync = NULL;
	conn->shutdown = NULL;
	conn->error = NULL;

	mowgli_node_add(conn, &conn->n, &all_conns);

	return conn;
}

void u_conn_destroy(u_conn *conn)
{
	if (conn->flags & U_CONN_DESTROY)
		return;
	conn->flags |= U_CONN_DESTROY;
	mowgli_node_delete(&conn->n, &all_conns);
	mowgli_node_add(conn, &conn->n, &need_destroy);
}

static void conn_destroy_real(u_conn *conn)
{
	mowgli_eventloop_t *ev = conn->poll->eventloop;
	int fd = conn->poll->fd;

	u_log(LG_VERBOSE, "conn %p (%s) being destroyed", conn,
	      conn->host[0] ? conn->host : conn->ip);
	call_shutdown(conn);

	if (conn->dnsq)
		mowgli_dns_delete_query(base_dns, conn->dnsq);
	if (conn->error)
		free(conn->error);
	free(conn->obuf);

	mowgli_pollable_destroy(ev, conn->poll);
	close(fd);
	free(conn);
}

static void call_shutdown(u_conn *conn)
{
	if (conn->flags & U_CONN_SHUTDOWN)
		return;
	conn->flags |= U_CONN_SHUTDOWN;
	if (conn->shutdown)
		conn->shutdown(conn);
}

void u_conn_shutdown(u_conn *conn)
{
	conn->flags |= U_CONN_CLOSING;
	if (conn->sync) conn->sync(conn);
}

void u_conn_fatal(u_conn *conn, char *reason)
{
	if (conn->error)
		free(conn->error);
	conn->error = strdup(reason);
	conn->flags |= U_CONN_CLOSING;
	u_conn_destroy(conn);
}

/* sadfaec */
void u_conn_obufsize(u_conn *conn, int obufsize)
{
	char *buf;

	if (conn->obufsize == obufsize)
		return;

	if (conn->obuflen > obufsize)
		conn->obuflen = obufsize;

	buf = malloc(obufsize);
	memcpy(buf, conn->obuf, conn->obuflen);
	free(conn->obuf);
	conn->obuf = buf;
	conn->obufsize = obufsize;
}

u_conn *u_conn_by_name(char *s)
{
	if (strchr(s, '.')) {
		u_server *sv = u_server_by_name(s);
		return sv ? sv->link : NULL;
	} else {
		u_user *u = u_user_by_nick(s);
		return u ? u->link : NULL;
	}
}

void conn_out_clear(u_conn *conn)
{
	char *s;

	s = (char*)memchr(conn->obuf, '\r', conn->obuflen);
	if (!s || *++s != '\n')
		s = conn->obuf;
	else
		s++;

	conn->obuflen = s - conn->obuf;
}

void u_conn_vf(u_conn *conn, const char *fmt, va_list va)
{
	int type;
	char buf[4096];
	char *p, *s, *end;

	if (!conn || (conn->flags & U_CONN_NO_SEND))
		return;

	p = conn->obuf + conn->obuflen;
	end = conn->obuf + conn->obufsize - 2; /* -2 for \r\n */

	switch (conn->ctx) {
	default:
		type = FMT_USER;
		break;
	case CTX_SERVER:
		type = FMT_SERVER;
		break;
	}

	vsnf(type, buf, 4096, fmt, va);
	buf[510] = '\0'; /* i guess it works... */

	u_log(LG_DEBUG, "[%G] <- %s", conn, buf);

	for (s=buf; p<end && *s;)
		*p++ = *s++;

	if (p >= end) {
		u_conn_fatal(conn, "Sendq full");
		return;
	}

	*p++ = '\r';
	*p++ = '\n';

	conn->obuflen = p - conn->obuf;

	if (conn->sync) conn->sync(conn);
}

void u_conn_f(u_conn *conn, char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	u_conn_vf(conn, fmt, va);
	va_end(va);
}

void u_conn_vnum(u_conn *conn, const char *tgt, int num, va_list va)
{
	char buf[4096];
	char *fmt;

	fmt = u_numeric_fmt[num];

	if (fmt == NULL) {
		u_log(LG_SEVERE, "Attempted to use NULL numeric %d", num);
		return;
	}

	/* numerics are ALWAYS FMT_USER */
	vsnf(FMT_USER, buf, 4096, fmt, va);

	u_conn_f(conn, ":%S %03d %s %s", &me, num, tgt, buf);
}

int u_conn_num(u_conn *conn, int num, ...)
{
	va_list va;

	va_start(va, num);
	switch (conn->ctx) {
	case CTX_NONE:
		u_conn_vnum(conn, "*", num, va);
		break;
	case CTX_USER:
		u_user_vnum(conn->priv, num, va);
		break;
	default:
		u_log(LG_SEVERE, "Can't use u_conn_num on context %d!", conn->ctx);
	}
	va_end(va);

	return 0;
}

static void u_conn_check_ping(u_conn *conn)
{
	int timeout, tdelta;

	if (!conn->auth || !conn->auth->cls)
		return;

	timeout = conn->auth->cls->timeout;
	tdelta = NOW.tv_sec - conn->last;

	if (tdelta > timeout / 2 && !(conn->flags & U_CONN_AWAIT_PONG)) {
		u_conn_f(conn, "PING :%s", me.name);
		conn->flags |= U_CONN_AWAIT_PONG;
	}

	if (tdelta > timeout)
		u_conn_fatal(conn, "Ping timeout");
}

void u_conn_check_ping_all(void *priv)
{
	mowgli_node_t *n, *tn;

	sync_time();

	MOWGLI_LIST_FOREACH_SAFE(n, tn, all_conns.head)
		u_conn_check_ping(n->data);
}

void u_conn_run(mowgli_eventloop_t *ev)
{
	mowgli_node_t *n, *tn;

	while (!ev->death_requested) {
		mowgli_eventloop_run_once(ev);

		MOWGLI_LIST_FOREACH_SAFE(n, tn, need_destroy.head) {
			mowgli_node_delete(n, &need_destroy);
			conn_destroy_real(n->data);
		}
	}
}

int init_conn(void)
{
	mowgli_list_init(&all_conns);

	mowgli_list_init(&need_destroy);

	return 0;
}

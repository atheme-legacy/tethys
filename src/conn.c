/* Tethys, conn.c -- line-based connection management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static mowgli_list_t all_conns;
static mowgli_list_t all_origs;

static mowgli_list_t need_destroy;

static mowgli_patricia_t *u_conf_listen_handlers = NULL;

static void origin_rdns();
static void origin_recv(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                        mowgli_eventloop_io_dir_t dir, void *priv);
static void toplev_recv(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                        mowgli_eventloop_io_dir_t dir, void *priv);

static void toplev_set_send(u_conn*, bool);
static void toplev_set_recv(u_conn*, bool);
static void toplev_check_set_send(u_conn*);

static void call_shutdown(u_conn*);

struct rdns_query {
	u_conn *conn;
	mowgli_dns_query_t q;
};

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

	conn->shutdown = NULL;
	conn->error = NULL;

	mowgli_node_add(conn, &conn->n, &all_conns);

	toplev_set_recv(conn, true);

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
	call_shutdown(conn);
	toplev_set_recv(conn, false);
	toplev_check_set_send(conn);
}

void u_conn_fatal(u_conn *conn, char *reason)
{
	if (conn->error)
		free(conn->error);
	conn->error = strdup(reason);
	conn->flags |= U_CONN_CLOSING;
	call_shutdown(conn);
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
		return sv ? sv->conn : NULL;
	} else {
		u_user *u = u_user_by_nick(s);
		return u ? u_user_conn(u) : NULL;
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
	buf[512] = '\0'; /* i guess it works... */

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

	toplev_check_set_send(conn);
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

u_conn_origin *u_conn_origin_create(mowgli_eventloop_t *ev, u_long addr,
                                    u_short port)
{
	struct sockaddr_in sa;
	u_conn_origin *orig;
	int fd, one=1;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		u_perror("socket");
		goto out;
	}

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = addr;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
		u_perror("setsockopt");
		goto out;
	}

	if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
		u_perror("bind");
		goto out;
	}

	if (listen(fd, 5) < 0) {
		u_perror("listen");
		goto out;
	}

	if (!(orig = malloc(sizeof(*orig))))
		goto out_close;

	if (!(orig->poll = mowgli_pollable_create(ev, fd, orig)))
		goto out_free;

	mowgli_pollable_set_nonblocking(orig->poll, true);
	mowgli_pollable_setselect(ev, orig->poll, MOWGLI_EVENTLOOP_IO_READ,
	                          origin_recv);

	mowgli_node_add(orig, &orig->n, &all_origs);

	return orig;

out_free:
	free(orig);
out_close:
	close(fd);
out:
	return NULL;
}

void u_conn_origin_destroy(u_conn_origin *orig)
{
	if (!orig || !orig->poll || !orig->poll->eventloop) {
		u_log(LG_ERROR, "can't destroy origin %p", orig);
		return;
	}

	close(orig->poll->fd);
	mowgli_node_delete(&orig->n, &all_origs);
	mowgli_pollable_destroy(orig->poll->eventloop, orig->poll);
	free(orig);
}

static void dispatch_lines(u_conn *conn)
{
	char buf[BUFSIZE], line[BUFSIZE];
	u_msg msg;
	int sz;

	while ((sz = u_linebuf_line(&conn->ibuf, buf, BUFSIZE)) != 0) {
		if (sz > 0)
			buf[sz] = '\0';
		if (sz < 0 || strlen(buf) != sz) {
			u_conn_fatal(conn, "Line buffer error");
			break;
		}
		u_log(LG_DEBUG, "[%G] -> %s", conn, buf);
		memcpy(line, buf, sz + 1);
		u_msg_parse(&msg, buf);
		u_cmd_invoke(conn, &msg, line);
		conn->last = NOW.tv_sec;
		conn->flags &= ~U_CONN_AWAIT_PONG;
	}
}

static void origin_rdns(mowgli_dns_reply_t *reply, int reason, void *vptr)
{
	struct rdns_query *query = vptr;
	u_conn *conn = query->conn;
	const char *reasonstr = "Unknown error";

	if (reply && reply->addr.addr.ss_family != AF_INET) {
		reply = NULL;
		reasonstr = "No IPv6 RDNS yet";
		reason = 0;
	}

	if (reply == NULL) {
		u_strlcpy(conn->host, conn->ip, U_CONN_HOSTSIZE);
		switch (reason) {
		case MOWGLI_DNS_RES_NXDOMAIN:
			reasonstr = "No such domain";
			break;
		case MOWGLI_DNS_RES_INVALID:
			reasonstr = "Invalid domain";
			break;
		case MOWGLI_DNS_RES_TIMEOUT:
			reasonstr = "Request timeout";
			break;
		}
		u_conn_f(conn, ":%s NOTICE * :*** Couldn't find your hostname: %s -- "
		         "using your ip: %s", me.name, reasonstr, conn->host);
	} else {
		/* XXX: Validate that rDNS matches Forward DNS. */
		u_strlcpy(conn->host, reply->h_name, U_CONN_HOSTSIZE);
		u_conn_f(conn, ":%s NOTICE * :*** Found your hostname: %s",
		         me.name, conn->host);
	}

	conn->dnsq = NULL;
	free(query);

	dispatch_lines(conn);
}

static void origin_recv(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                        mowgli_eventloop_io_dir_t dir, void *priv)
{
	mowgli_eventloop_pollable_t *poll = mowgli_eventloop_io_pollable(io);
	struct rdns_query *query;
	u_conn *conn;
	struct sockaddr_storage addr;
	void *addrptr;
	socklen_t addrlen = sizeof(addr);
	int fd;

	sync_time();

	if ((fd = accept(poll->fd, (struct sockaddr*)&addr, &addrlen)) < 0) {
		u_perror("accept");
		return;
	}

	conn = u_conn_create(ev, fd);

	addrptr = NULL;
	switch (addr.ss_family) {
	case AF_INET:
		addrptr = &((struct sockaddr_in*)&addr)->sin_addr;
		break;
	case AF_INET6:
		addrptr = &((struct sockaddr_in6*)&addr)->sin6_addr;
		break;
	}
	inet_ntop(addr.ss_family, addrptr, conn->ip, sizeof(conn->ip));

	u_conn_f(conn, ":%s NOTICE * :*** Looking up your hostname", me.name);
	query = malloc(sizeof(*query));
	query->conn = conn;
	query->q.ptr = query;
	query->q.callback = origin_rdns;
	conn->dnsq = &query->q;
	mowgli_dns_gethost_byaddr(base_dns, (void*)&addr, conn->dnsq);

	u_log(LG_VERBOSE, "Connection from %s", conn->ip);
}

static void toplev_recv(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                        mowgli_eventloop_io_dir_t dir, void *priv)
{
	u_conn *conn = priv;
	char buf[1024];
	int sz;

	sync_time();

	sz = recv(conn->poll->fd, buf, 1024-conn->ibuf.pos, 0);

	if (sz <= 0) {
		if (sz < 0)
			u_perror("recv");
		u_conn_fatal(conn, sz == 0 ? "End of stream" : "Read error");
		return;
	}

	if (u_linebuf_data(&conn->ibuf, buf, sz) < 0) {
		u_conn_fatal(conn, "Excess flood");
		return;
	}

	if (conn->host[0])
		dispatch_lines(conn);
}

static void toplev_send(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                        mowgli_eventloop_io_dir_t dir, void *priv)
{
	u_conn *conn = priv;
	int sz;

	sync_time();

	sz = send(conn->poll->fd, conn->obuf, conn->obuflen, 0);

	if (sz < 0) {
		/* TODO: determine if error is recoverable */
		u_perror("send");
		u_conn_fatal(conn, "Write error");
		return;
	}

	if (sz > 0) {
		memmove(conn->obuf, conn->obuf + sz, conn->obufsize - sz);
		conn->obuflen -= sz;
	}

	toplev_check_set_send(conn);
}

static inline void toplev_set(u_conn *conn, bool en,
                              mowgli_eventloop_io_dir_t dir,
                              mowgli_eventloop_io_cb_t *cb)
{
	mowgli_eventloop_t *ev = conn->poll->eventloop;

	mowgli_pollable_setselect(ev, conn->poll, dir, en ? cb : NULL);
}

static void toplev_set_recv(u_conn *conn, bool en)
{
	toplev_set(conn, en, MOWGLI_EVENTLOOP_IO_READ, toplev_recv);
}

static void toplev_set_send(u_conn *conn, bool en)
{
	toplev_set(conn, en, MOWGLI_EVENTLOOP_IO_WRITE, toplev_send);
}

static void toplev_check_set_send(u_conn *conn)
{
	if (conn->obuflen == 0 || conn->error) {
		toplev_set_send(conn, false);
		if (conn->flags & U_CONN_CLOSING)
			u_conn_destroy(conn);
	} else {
		toplev_set_send(conn, true);
	}
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

static void *conf_end(void *unused, void *unused2)
{
	if (all_origs.count != 0)
		return NULL;

	u_log(LG_WARN, "No listeners! Opening one on 6667");
	u_conn_origin_create(base_ev, INADDR_ANY, 6667);

	return NULL;
}

static void conf_listen(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_conf_traverse(cf, ce->entries, u_conf_listen_handlers);
}

static void conf_listen_port(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	ushort low, hi;
	char buf[512];
	char *s, *lows, *his;

	mowgli_strlcpy(buf, ce->vardata, sizeof buf);

	lows = buf;
	his = NULL;

	if ((s = strstr(buf, "..")) || (s = strstr(buf, "-"))) {
		*s++ = '\0';
		while (*s && !isdigit(*s))
			s++;
		his = s;
	}

	low = atoi(lows);
	hi = (his && *his) ? atoi(his) : low;

	if (low == 0 || hi == 0) {
		u_log(LG_ERROR, "%s: invalid listen range string", ce->vardata);
		return;
	}

	if (hi < low) {
		u_log(LG_ERROR, "%u-%u: invalid listen range", low, hi);
		return;
	}

	if (hi - low > 20) {
		u_log(LG_ERROR, "%u-%u: listener range too large", low, hi);
		return;
	}

	for (; low <= hi; low++) {
		u_log(LG_DEBUG, "Listening on %u", low);
		u_conn_origin_create(base_ev, INADDR_ANY, low);
	}
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
	mowgli_list_init(&all_origs);

	mowgli_list_init(&need_destroy);

	u_hook_add(HOOK_CONF_END, conf_end, NULL);
	u_conf_add_handler("listen", conf_listen, NULL);

	u_conf_listen_handlers = mowgli_patricia_create(ascii_canonize);
	u_conf_add_handler("port", conf_listen_port, u_conf_listen_handlers);

	return 0;
}

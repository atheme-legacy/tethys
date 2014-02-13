/* Tethys, toplev.c -- toplevel IRC connection operations
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static mowgli_list_t all_origs;

static mowgli_patricia_t *u_conf_listen_handlers = NULL;

static void origin_rdns();
static void origin_recv(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                        mowgli_eventloop_io_dir_t dir, void *priv);

static void toplev_recv(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                        mowgli_eventloop_io_dir_t dir, void *priv);

static void toplev_set_send(u_conn*, bool);
static void toplev_set_recv(u_conn*, bool);
static void toplev_check_set_send(u_conn*);

struct rdns_query {
	u_conn *conn;
	mowgli_dns_query_t q;
};

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

static void toplev_sync(u_conn *conn)
{
	if (conn->flags & U_CONN_CLOSING)
		toplev_set_recv(conn, false);

	toplev_check_set_send(conn);
}

void u_toplev_attach(u_conn *conn)
{
	toplev_set_recv(conn, true);
	toplev_check_set_send(conn);

	conn->sync = toplev_sync;
}

static void start_rdns(u_conn *conn, struct sockaddr *addr, socklen_t addrlen)
{
	struct rdns_query *query;
	void *addrptr;

	addrptr = NULL;
	switch (addr->sa_family) {
	case AF_INET:
		addrptr = &((struct sockaddr_in*)addr)->sin_addr;
		break;
	case AF_INET6:
		addrptr = &((struct sockaddr_in6*)addr)->sin6_addr;
		break;
	}

	/* also highly improbable that addr is neither AF_INET nor AF_INET6,
	   but we want to be 100% CERTAIN, so that the static analysis
	   tools will leave us alone. let's pretend they are connecting from
	   localhost --aji */
	if (addrptr != NULL)
		inet_ntop(addr->sa_family, addrptr, conn->ip, sizeof(conn->ip));
	else
		u_strlcpy(conn->ip, "127.0.0.1", INET6_ADDRSTRLEN);

	u_conn_f(conn, ":%s NOTICE * :*** Looking up your hostname", me.name);
	query = malloc(sizeof(*query));
	query->conn = conn;
	query->q.ptr = query;
	query->q.callback = origin_rdns;
	conn->dnsq = &query->q;
	mowgli_dns_gethost_byaddr(base_dns, (void*)addr, conn->dnsq);
}

static void toplev_connect_ready(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                                 mowgli_eventloop_io_dir_t dir, void *priv)
{
	u_conn *conn = priv;
	int fd = conn->poll->fd;
	void (*cb)(u_conn *conn, int error) = conn->priv;
	socklen_t len;
	int e;

	toplev_set(conn, false, MOWGLI_EVENTLOOP_IO_WRITE, NULL);

	len = sizeof(e);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &len) < 0) {
		cb(conn, errno);
		u_conn_fatal(conn, "getsockopt() error");
		return;
	}

	if (e != 0) {
		cb(conn, e);
		u_conn_fatal(conn, "connection error");
		return;
	}

	u_log(LG_VERBOSE, "Connected to %s", conn->ip);

	u_toplev_attach(conn);
	cb(conn, 0);
}

void u_toplev_connect(mowgli_eventloop_t *ev, struct sockaddr *sa,
                      socklen_t len, void (*cb)(u_conn*, int))
{
	int fd = -1, flags;
	u_conn *conn;

	if ((fd = socket(sa->sa_family, SOCK_STREAM, 0)) < 0)
		goto error;

	if ((flags = fcntl(fd, F_GETFL)) < 0)
		goto error;

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		goto error;

	if (connect(fd, sa, len) < 0) {
		if (errno != EINPROGRESS)
			goto error;
	}

	if (!(conn = u_conn_create(ev, fd))) {
		/* kind of cheating, with errno here. */
		errno = EINVAL;
		goto error;
	}

	conn->priv = cb;
	start_rdns(conn, sa, len);

	u_log(LG_VERBOSE, "Connecting to %s", conn->ip);

	toplev_set(conn, true, MOWGLI_EVENTLOOP_IO_WRITE, toplev_connect_ready);

	return;

error:
	if (fd >= 0)
		close(fd);
	cb(NULL, errno);
	return;
}

u_toplev_origin *u_toplev_origin_create(mowgli_eventloop_t *ev, u_long addr,
                                    u_short port)
{
	struct sockaddr_in sa;
	u_toplev_origin *orig;
	int fd, one=1;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		u_perror("socket");
		goto out;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = addr;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
		u_perror("setsockopt");
		goto out_close;
	}

	if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
		u_perror("bind");
		goto out_close;
	}

	if (listen(fd, 5) < 0) {
		u_perror("listen");
		goto out_close;
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

void u_toplev_origin_destroy(u_toplev_origin *orig)
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
		u_conn_f(conn, ":%s NOTICE * :*** Couldn't find your hostname: "
                         "%s -- using your ip: %s", me.name, reasonstr, conn->host);
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
	struct sockaddr_storage addr;
	socklen_t addrlen;
	u_conn *conn;
	int fd;

	sync_time();

	addrlen = sizeof(addr);
	if ((fd = accept(poll->fd, (struct sockaddr*)&addr, &addrlen)) < 0) {
		u_perror("accept");
		return;
	}

	conn = u_conn_create(ev, fd);
	start_rdns(conn, (struct sockaddr*)&addr, addrlen);

	u_toplev_attach(conn);

	u_log(LG_VERBOSE, "Connection from %s", conn->ip);
}

static void *conf_end(void *unused, void *unused2)
{
	if (all_origs.count != 0)
		return NULL;

	u_log(LG_WARN, "No listeners! Opening one on 6667");
	u_toplev_origin_create(base_ev, INADDR_ANY, 6667);

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
		u_toplev_origin_create(base_ev, INADDR_ANY, low);
	}
}

int init_toplev(void)
{
	mowgli_list_init(&all_origs);

	u_hook_add(HOOK_CONF_END, conf_end, NULL);
	u_conf_add_handler("listen", conf_listen, NULL);

	u_conf_listen_handlers = mowgli_patricia_create(ascii_canonize);
	u_conf_add_handler("port", conf_listen_port, u_conf_listen_handlers);

	return 0;
}

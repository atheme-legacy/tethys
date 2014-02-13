/* Tethys, conn.c -- stream connection management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* globals */

static mowgli_list_t awaiting_cleanup;

/* forward declarations */

static void rdns_start(u_conn*, const struct sockaddr*, socklen_t);

static void connect_end(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                        mowgli_eventloop_io_dir_t dir, void *priv);
static void recv_ready(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                       mowgli_eventloop_io_dir_t dir, void *priv);
static void send_ready(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                       mowgli_eventloop_io_dir_t dir, void *priv);

static void set_recv(u_conn *conn, mowgli_eventloop_io_cb_t *cb);
static void set_send(u_conn *conn, mowgli_eventloop_io_cb_t *cb);

static void sync_on_update(u_conn *conn);

/* connection creation and shutdown */
/* -------------------------------- */

static u_conn *conn_create(mowgli_eventloop_t *ev, u_conn_ctx *ctx,
                           void *priv, int fd,
                           const struct sockaddr *sa, socklen_t alen)
{
	u_conn *conn;
	void *addr;

	conn = calloc(1, sizeof(*conn));

	conn->state = U_CONN_INVALID;

	conn->poll = mowgli_pollable_create(ev, fd, conn);

	switch (sa->sa_family) {
	case AF_INET:
		addr = &((struct sockaddr_in*)sa)->sin_addr;
		break;
	case AF_INET6:
		addr = &((struct sockaddr_in6*)sa)->sin6_addr;
		break;
	default:
		addr = NULL;
		u_log(LG_ERROR, "conn_create: Unk. a.f. %d", sa->sa_family);
		break;
	}

	if (addr) {
		inet_ntop(sa->sa_family, addr, conn->ip, sizeof(conn->ip));
	} else {
		/* this is not the best thing to do, but whatever */
		u_strlcpy(conn->ip, "127.0.0.1", sizeof(conn->ip));
	}

	conn->obufsize = 32 << 10;
	conn->obuf = malloc(conn->obufsize);

	conn->ctx = ctx;
	conn->priv = priv;

	return conn;
}

static void final_cleanup(u_conn *conn)
{
	mowgli_eventloop_t *ev = conn->poll->eventloop;
	int fd = conn->poll->fd;

	if (conn->ctx->cleanup)
		conn->ctx->cleanup(conn);

	if (conn->dnsq)
		mowgli_dns_delete_query(base_dns, conn->dnsq);
	free(conn->obuf);

	mowgli_pollable_destroy(ev, conn->poll);
	close(fd);
	free(conn);
}

static int make_nonblocking(int fd)
{
	int fops;

	if ((fops = fcntl(fd, F_GETFL)) < 0) {
		u_perror("fcntl");
		return -1;
	}

	if (fcntl(fd, F_SETFL, fops | O_NONBLOCK) < 0) {
		u_perror("fcntl");
		return -1;
	}

	return 0;
}

static int start_nonblocking_connect(const struct sockaddr *sa, socklen_t alen)
{
	int fd;

	if ((fd = socket(sa->sa_family, SOCK_STREAM, 0)) < 0) {
		u_perror("socket");
		return -1;
	}

	if (make_nonblocking(fd) < 0) {
		close(fd);
		return -1;
	}

	if (connect(fd, sa, alen) < 0 && errno != EINPROGRESS) {
		u_perror("connect");
		close(fd);
		return -1;
	}

	return fd;
}

u_conn *u_conn_accept(mowgli_eventloop_t *ev, u_conn_ctx *ctx, void *priv,
                      ulong flags, int listener)
{
	int fd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	u_conn *conn;

	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);

	if ((fd = accept(listener, (struct sockaddr*)&addr, &addrlen)) < 0) {
		u_perror("accept");
		return NULL;
	}

	if (make_nonblocking(fd) < 0) {
		close(fd);
		return NULL;
	}

	conn = conn_create(ev, ctx, priv, fd, (struct sockaddr*)&addr, addrlen);
	conn->state = U_CONN_ACTIVE;

	set_recv(conn, recv_ready);

	rdns_start(conn, (struct sockaddr*)&addr, addrlen);

	return conn;
}

u_conn *u_conn_connect(mowgli_eventloop_t *ev, u_conn_ctx *ctx, void *priv,
                       ulong flags, const struct sockaddr *sa, socklen_t alen)
{
	int fd;
	u_conn *conn;

	if ((fd = start_nonblocking_connect(sa, alen)) < 0)
		return NULL;

	conn = conn_create(ev, ctx, priv, fd, sa, alen);
	conn->state = U_CONN_CONNECTING;

	set_send(conn, connect_end);

	rdns_start(conn, sa, alen);

	return conn;
}

void u_conn_shut_down(u_conn *conn)
{
	conn->state = U_CONN_SHUTTING_DOWN;

	set_recv(conn, NULL);
	set_send(conn, send_ready);

	sync_on_update(conn);
}

static void mark_for_cleanup(u_conn *conn)
{
	conn->state = U_CONN_AWAIT_CLEANUP;

	set_recv(conn, NULL);
	set_send(conn, NULL);

	sync_on_update(conn);

	mowgli_node_add(conn, &conn->n, &awaiting_cleanup);
}

static void fatal_error(u_conn *conn, const char *msg, int err)
{
	if (conn->state == U_CONN_ACTIVE) {
		if (conn->ctx->fatal_error)
			conn->ctx->fatal_error(conn, msg, err);
	} else {
		u_log(LG_ERROR, "connection error: %s", msg);
	}

	mark_for_cleanup(conn);
}

/* RDNS */
/* ---- */

struct rdns_query {
	u_conn *conn;
	mowgli_dns_query_t q;
};

static void rdns_callback(mowgli_dns_reply_t *reply, int reason, void *vptr)
{
	struct rdns_query *q = vptr;
	u_conn *conn = q->conn;
	const char *rstr = "Unknown error";

	if (reply && reply->addr.addr.ss_family != AF_INET) {
		/* XXX: but why? */
		reply = NULL;
		rstr = "IPv4 RDNS only";
		reason = 0;
	}

	if (reply == NULL) {
		u_strlcpy(conn->host, conn->ip, U_CONN_HOSTSIZE);

		switch (reason) {
		case MOWGLI_DNS_RES_NXDOMAIN:
			rstr = "No such domain";
			break;
		case MOWGLI_DNS_RES_INVALID:
			rstr = "Invalid domain";
			break;
		case MOWGLI_DNS_RES_TIMEOUT:
			rstr = "Request timeout";
			break;
		}
	} else {
		/* TODO: initiate forward DNS check here */

		u_strlcpy(conn->host, reply->h_name, U_CONN_HOSTSIZE);
		rstr = NULL;
	}

	conn->dnsq = NULL;
	free(q);

	if (conn->ctx->rdns_finish != NULL)
		conn->ctx->rdns_finish(conn, rstr);
}

static void rdns_start(u_conn *conn, const struct sockaddr *sa, socklen_t alen)
{
	struct rdns_query *q;

	q = calloc(1, sizeof(*q));
	q->conn = conn;
	q->q.ptr = q;
	q->q.callback = rdns_callback;

	conn->dnsq = &q->q;

	mowgli_dns_gethost_byaddr(base_dns, (struct sockaddr_storage*)sa,
	                          conn->dnsq);

	if (conn->ctx->rdns_start != NULL)
		conn->ctx->rdns_start(conn);
}

/* User data transfer API */
/* ---------------------- */

static inline bool recv_permitted(u_conn *conn)
{
	if (conn == NULL || conn->state != U_CONN_ACTIVE)
		return false;

	return true;
}

static inline bool send_permitted(u_conn *conn)
{
	if (conn == NULL ||
	    conn->state == U_CONN_SHUTTING_DOWN ||
	    conn->state == U_CONN_AWAIT_CLEANUP)
		return false;

	return true;
}

ssize_t u_conn_recv(u_conn *conn, uchar *data, size_t sz)
{
	ssize_t rsz;

	if (!recv_permitted(conn))
		return 0;

	rsz = read(conn->poll->fd, data, sz);

	if (rsz < 0) {
		int e = errno;

		/* TODO: determine if error is recoverable */
		u_perror("read");

		fatal_error(conn, "Read error", e);

	} else if (rsz == 0) {
		if (conn->ctx->end_of_stream)
			conn->ctx->end_of_stream(conn);

		u_conn_shut_down(conn);
	}

	return rsz;
}

ssize_t u_conn_send(u_conn *conn, const uchar *data, size_t sz)
{
	if (!send_permitted(conn))
		return 0;

	if (conn->obuflen + sz > conn->obufsize)
		sz = conn->obufsize - conn->obuflen;

	memcpy(conn->obuf + conn->obuflen, data, sz);
	conn->obuflen += sz;

	sync_on_update(conn);

	return sz;
}

/* mowgli eventloop callbacks */
/* -------------------------- */

static void null_connect_finish(u_conn *conn, int err)
{
	if (err != 0)
		u_log(LG_ERROR, "Unhandled connect error %d", err);
}

static void connect_end(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                        mowgli_eventloop_io_dir_t dir, void *priv)
{
	u_conn *conn = priv;
	socklen_t len;
	void (*cb)(u_conn *conn, int err);
	int e;

	cb = null_connect_finish;
	if (conn->ctx->connect_finish != NULL)
		cb = conn->ctx->connect_finish;

	len = sizeof(e);
	if (getsockopt(conn->poll->fd, SOL_SOCKET, SO_ERROR, &e, &len) < 0) {
		e = errno;
		cb(conn, e);
		mark_for_cleanup(conn);
		return;
	}

	if (e != 0) {
		cb(conn, e);
		mark_for_cleanup(conn);
		return;
	}

	conn->state = U_CONN_ACTIVE;

	cb(conn, 0);

	sync_on_update(conn);
}

static void recv_ready(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                       mowgli_eventloop_io_dir_t dir, void *priv)
{
	u_conn *conn = priv;

	if (conn->ctx->data_ready != NULL)
		conn->ctx->data_ready(conn);
}

static void send_ready(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                       mowgli_eventloop_io_dir_t dir, void *priv)
{
	u_conn *conn = priv;
	ssize_t sz;

	sz = write(conn->poll->fd, conn->obuf, conn->obuflen);

	if (sz < 0) {
		int e = errno;

		/* TODO: determine if error is recoverable */
		u_perror("send");

		fatal_error(conn, "Write error", e);
		return;
	}

	if (sz > 0) {
		memmove(conn->obuf, conn->obuf + sz, conn->obufsize - sz);
		conn->obuflen -= sz;
	}

	sync_on_update(conn);
}

static void set_recv(u_conn *conn, mowgli_eventloop_io_cb_t *cb)
{
	mowgli_pollable_setselect(conn->poll->eventloop, conn->poll,
	                          MOWGLI_EVENTLOOP_IO_READ, cb);
}

static void set_send(u_conn *conn, mowgli_eventloop_io_cb_t *cb)
{
	mowgli_pollable_setselect(conn->poll->eventloop, conn->poll,
	                          MOWGLI_EVENTLOOP_IO_WRITE, cb);
}

static void sync_on_update(u_conn *conn)
{
	bool use_recv = false;

	switch (conn->state) {
	case U_CONN_ACTIVE:
		use_recv = true;
		set_send(conn, conn->obuflen > 0 ? send_ready : NULL);
		break;

	case U_CONN_SHUTTING_DOWN:
		if (conn->obuflen == 0) {
			set_send(conn, NULL);
			mark_for_cleanup(conn);
		}
		break;

	case U_CONN_INVALID:
	case U_CONN_CONNECTING:
	case U_CONN_AWAIT_CLEANUP:
	default:
		break;
	}

	set_recv(conn, use_recv ? recv_ready : NULL);
}

/* main() API */
/* ---------- */

void u_conn_run(mowgli_eventloop_t *ev)
{
	mowgli_node_t *n, *tn;

	while (!ev->death_requested) {
		mowgli_eventloop_run_once(ev);

		MOWGLI_LIST_FOREACH_SAFE(n, tn, awaiting_cleanup.head) {
			u_conn *conn = n->data;
			final_cleanup(conn);
			mowgli_node_delete(n, &awaiting_cleanup);
		}
	}
}

int init_conn(void)
{
	mowgli_list_init(&awaiting_cleanup);

	return 0;
}

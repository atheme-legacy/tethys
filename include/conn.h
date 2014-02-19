/* Tethys, conn.h -- stream connection management
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_CONN_H__
#define __INC_CONN_H__

#define U_CONN_HOSTSIZE 256

typedef struct u_conn_ctx u_conn_ctx;
typedef enum u_conn_state u_conn_state;
typedef struct u_conn u_conn;

struct u_conn_ctx {
	void (*attach)(u_conn*);

	/* connect_finish: leaving CONNECTING.
	   fatal_error: entering AWAIT_CLEANUP directly from ACTIVE.
	   cleanup: leaving AWAIT_CLEANUP, deleting connection. */

	void (*connect_finish)(u_conn*, int err);
	void (*fatal_error)(u_conn*, const char*, int err);
	void (*cleanup)(u_conn*);

	void (*data_ready)(u_conn*);
	void (*end_of_stream)(u_conn*);
	void (*rdns_start)(u_conn*);
	void (*rdns_finish)(u_conn*, const char*);
};

enum u_conn_state {
	/* This is not a valid state. */
	U_CONN_INVALID,

	/* The connection has not been established yet. The context will
	   be notified when the connection finishes. */
	U_CONN_CONNECTING,

	/* The connection is active. Data can be sent and received. */
	U_CONN_ACTIVE,

	/* The connection is being shut down. Data cannot be sent or
	   received. The connection will be destroyed when the send
	   queue is empty */
	U_CONN_SHUTTING_DOWN,

	/* The connection's send queue has just been emptied, or an
	   unrecoverable connection-related error has occurred. The
	   connection is unusable and will be deleted soon. */
	U_CONN_AWAIT_CLEANUP,
};

struct u_conn {
	mowgli_node_t n;

	u_conn_state state;

	mowgli_eventloop_pollable_t *poll;
	char ip[INET6_ADDRSTRLEN];
	char host[U_CONN_HOSTSIZE];
	mowgli_dns_query_t *dnsq;

	u_sendq sendq;

	u_conn_ctx *ctx;
	void *priv;
};

extern u_conn *u_conn_accept(mowgli_eventloop_t*, u_conn_ctx*, void*,
                             ulong flags, int listener);

extern u_conn *u_conn_connect(mowgli_eventloop_t*, u_conn_ctx*, void*,
                              ulong flags, const struct sockaddr*, socklen_t);

extern void u_conn_shut_down(u_conn*);

extern ssize_t u_conn_recv(u_conn*, uchar*, size_t sz);
extern ssize_t u_conn_send(u_conn*, const uchar*, size_t sz);

/* to allow vsnf, sprintf, etc. directly into the send queue */
extern uchar *u_conn_get_send_buffer(u_conn*, size_t sz);
extern size_t u_conn_end_send_buffer(u_conn*, size_t sz);

extern void u_conn_sendq_clear(u_conn*);

extern void u_conn_run(mowgli_eventloop_t *ev);

extern int init_conn(void);

extern mowgli_json_t *u_conn_to_json(u_conn *conn);
extern u_conn *u_conn_from_json(
  mowgli_eventloop_t *ev,
  u_conn_ctx *ctx,
  void *priv,
  mowgli_json_t *jc);

#endif

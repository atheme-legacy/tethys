/* Tethys, echo.c -- Test of conn API with echo
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

mowgli_eventloop_t *base_ev;
mowgli_dns_t *base_dns;

static void fmt(u_conn *conn, const char *f, ...)
{
	va_list va;
	char buf[512];
	size_t sz;

	va_start(va, f);
	sz = vsnprintf(buf, 512, f, va);
	va_end(va);

	u_conn_send(conn, buf, sz);
}

static void echo_connect_finish(u_conn *conn, int err)
{
	printf("connect %s\n", err ? "failed" : "succeeded");
	fmt(conn, "connected!\n");
}

static void echo_fatal_error(u_conn *conn, const char *msg, int err)
{
	printf("fatal error: %s\n", msg);
}

static void echo_cleanup(u_conn *conn)
{
	printf("being cleaned up\n");
}

static void echo_data_ready(u_conn *conn)
{
	char buf[512];
	ssize_t sz;

	printf("data ready\n");

	if ((sz = u_conn_recv(conn, buf, 511)) > 0) {
		u_conn_send(conn, buf, sz);
		buf[sz] = '\0';
		if (!strcmp(buf, "quit\n")) {
			fmt(conn, "bye bye\n");
			u_conn_shut_down(conn);
		}
	}
}

static void echo_end_of_stream(u_conn *conn)
{
	printf("end of stream\n");
	fmt(conn, "end of stream :3\n");
}

static void echo_rdns_start(u_conn *conn)
{
	printf("rdns starting: ip=%s\n", conn->ip);
	fmt(conn, "rdns-ing %s\n", conn->ip);
}

static void echo_rdns_finish(u_conn *conn, const char *msg)
{
	printf("rdns finished: host=%s\n", conn->host);
	fmt(conn, "your host is %s\n", conn->host);
}

static u_conn_ctx echo_ctx = {
	.connect_finish = echo_connect_finish,
	.fatal_error = echo_fatal_error,
	.cleanup = echo_cleanup,

	.data_ready = echo_data_ready,
	.end_of_stream = echo_end_of_stream,
	.rdns_start = echo_rdns_start,
	.rdns_finish = echo_rdns_finish,
};

static int one = 1;

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	u_conn *conn;

	base_ev = mowgli_eventloop_create();
	base_dns = mowgli_dns_create(base_ev, MOWGLI_DNS_TYPE_ASYNC);

	if (init_conn() < 0) {
		printf("init_conn failed\n");
		return 0;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5050);

	if (argc > 1) {
		inet_pton(AF_INET, argv[1], &addr.sin_addr);
		conn = u_conn_connect(base_ev, &echo_ctx, NULL, 0,
		                      (struct sockaddr*)&addr, sizeof(addr));
		printf("connecting to %s on port 5050\n", conn->ip);
		fmt(conn, "connecting...\n");
	} else {
		int cli, f = socket(AF_INET, SOCK_STREAM, 0);
		setsockopt(f, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
		addr.sin_addr.s_addr = INADDR_ANY;
		bind(f, (struct sockaddr*)&addr, sizeof(addr));
		listen(f, 5);
		printf("listening for connection on port 5050...\n");
		conn = u_conn_accept(base_ev, &echo_ctx, NULL, 0, f);
		close(f);
		printf("got one!\n");
		fmt(conn, "hi\n");
	}

	u_conn_run(base_ev);

	printf("done\n");

	return 0;
}

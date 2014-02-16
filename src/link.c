/* Tethys, link.c -- IRC connections
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_link *link_create(u_conn *conn)
{
	u_link *link;

	link = calloc(1, sizeof(*link));
	link->conn = conn;

	return link;
}

static void link_destroy(u_link *link)
{
	if (link->pass != NULL)
		free(link->pass);

	free(link);
}

#define RECV_SIZE (16 << 10)

static void exceptional_quit(u_link *link, char *msg, ...);
static void dispatch_lines(u_link*);

static void on_connect_finish(u_conn *conn, int err)
{
	u_log(LG_VERBOSE, "link: connect finished");
}

static void on_fatal_error(u_conn *conn, const char *msg, int err)
{
	exceptional_quit(conn->priv, "%s: %s", msg, strerror(err));
}

static void on_cleanup(u_conn *conn)
{
	u_log(LG_VERBOSE, "link: being cleaned up");
	u_link_detach(conn->priv);
	conn->priv = NULL;
}

static void on_excess_flood(u_conn *conn)
{
	exceptional_quit(conn->priv, "Excess flood");
	u_link_f(conn->priv, "ERROR :Excess flood");

	u_conn_shut_down(conn);
}

static void on_sendq_full(u_conn *conn)
{
	exceptional_quit(conn->priv, "SendQ full");

	u_conn_shut_down(conn);
}

static void on_data_ready(u_conn *conn)
{
	u_link *link = conn->priv;
	size_t sz;

	if (link->ibuflen == IBUFSIZE) {
		on_excess_flood(conn);
		return;
	}

	sz = u_conn_recv(conn, link->ibuf + link->ibuflen,
	                 IBUFSIZE - link->ibuflen);

	if (sz <= 0)
		return;

	link->ibuflen += sz;

	dispatch_lines(link);
}

static void on_end_of_stream(u_conn *conn)
{
	exceptional_quit(conn->priv, "End of stream");
}

static void on_rdns_start(u_conn *conn)
{
	u_link *link = conn->priv;

	u_link_f(link, ":%S NOTICE * :*** Looking up your hostname", &me);
	link->flags |= U_LINK_WAIT_RDNS;
}

static void on_rdns_finish(u_conn *conn, const char *msg)
{
	u_link *link = conn->priv;

	if (msg != NULL) {
		u_link_f(link, ":%S NOTICE * :*** Couldn't find your hostname: "
		         "%s -- using your ip: %s", &me, msg, conn->host);
	} else {
		u_link_f(link, ":%S NOTICE * :*** Found your hostname: %s",
		         &me, conn->host);
	}

	link->flags &= ~U_LINK_WAIT_RDNS;

	dispatch_lines(link);
}

u_conn_ctx u_link_conn_ctx = {
	.connect_finish   = on_connect_finish,
	.fatal_error      = on_fatal_error,
	.cleanup          = on_cleanup,

	.data_ready       = on_data_ready,
	.end_of_stream    = on_end_of_stream,
	.rdns_start       = on_rdns_start,
	.rdns_finish      = on_rdns_finish,
};

static void exceptional_quit(u_link *link, char *msg, ...)
{
	char buf[512];
	va_list va;

	if (link->flags & U_LINK_SENT_QUIT)
		return;

	va_start(va, msg);
	vsnprintf(buf, 512, msg, va);
	va_end(va);

	/* TODO: send QUIT or SQUIT */

	link->flags |= U_LINK_SENT_QUIT;
}

static void dispatch_lines(u_link *link)
{
	uchar *buf;
	size_t buflen;
	uchar *s, *p;
	u_msg msg;

	buf = link->ibuf;
	buflen = link->ibuflen;

	/* i don't really ever comment my code, as it's mostly very straight
	   forward and relatively self-documenting. but C string processing
	   is typically very hairy stuff and it can be difficult to decipher
	   the intent of a chunk of code, so i commented it  --aji */

	buf[buflen] = '\0';

	while (buflen > 0) {
		/* check wait flags on every iteration, as line dispatch
		   can affect this */
		if (link->flags & U_LINK_WAIT)
			break;

		/* find the next \r and \n */
		s = memchr(buf, '\r', buflen);
		p = memchr(buf, '\n', buflen);

		/* if no line endings in buffer, we're done */
		if (!s && !p)
			break;

		/* if p is closer than s, then put p in s */
		if (!s || (p && p < s))
			s = p;
		/* s now contains the closest line ending */

		/* delete all contiguous line endings at s */
		for (p = s; *p == '\r' || *p == '\n'; p++)
			*p = '\0';
		/* p now points at the start of the next line */

		s = buf;
		buf = p;
		buflen = buflen - (p - s);
		/* s now points to the current line, and buf and buflen
		   describe the rest of the buffer */

		/* dispatch the line */
		if (u_msg_parse(&msg, (char*)s) < 0)
			continue;
		u_cmd_invoke(link, &msg, (char*)s);
	}

	/* move remaining buffer contents to the start of the in buffer */
	memmove(link->ibuf, link->ibuf + link->ibuflen - buflen, buflen);
	link->ibuflen = buflen;
}

void u_link_attach(u_conn *conn)
{
	conn->ctx = &u_link_conn_ctx;
	conn->priv = link_create(conn);
}

void u_link_detach(u_conn *conn)
{
	link_destroy(conn->priv);
	conn->ctx = NULL;
	conn->priv = NULL;
}

void u_link_f(u_link *link, const char *fmt, ...)
{
	uchar *buf;
	size_t sz;
	va_list va;
	int type;

	buf = u_conn_get_send_buffer(link->conn, 514);

	if (buf == NULL) {
		on_sendq_full(link->priv);
		return;
	}

	type = FMT_USER;
	if (link->type == LINK_SERVER)
		type = FMT_SERVER;

	va_start(va, fmt);
	sz = vsnf(type, (char*)buf, 512, fmt, va);
	va_end(va);

	buf[sz++] = '\r';
	buf[sz++] = '\n';

	u_conn_end_send_buffer(link->conn, sz);
}

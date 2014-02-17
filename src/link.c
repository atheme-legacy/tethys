/* Tethys, link.c -- IRC connections
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_link *link_create(void)
{
	u_link *link;

	link = calloc(1, sizeof(*link));

	return link;
}

static void link_destroy(u_link *link)
{
	if (link->pass != NULL)
		free(link->pass);

	free(link);
}

/* conn interaction */
/* ---------------- */

static void exceptional_quit(u_link *link, char *msg, ...);
static void dispatch_lines(u_link*);

static void on_attach(u_conn *conn)
{
	u_link *link = conn->priv;

	link->conn = conn;
}

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

	if (conn->priv) {
		link_destroy(conn->priv);
		conn->priv = NULL;
	}
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
	ssize_t sz;

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
	.attach           = on_attach,

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

	link->flags |= U_LINK_SENT_QUIT;

	switch (link->type) {
	case LINK_USER:
		u_sendto_visible(link->priv, ST_USERS, ":%H QUIT :%s",
		                 link->priv, buf);
		u_sendto_servers(NULL, ":%H QUIT :%s", link->priv, buf);
		u_user_destroy(link->priv);
		break;

	case LINK_SERVER:
		u_sendto_servers(NULL, ":%S SQUIT %S :%s", &me, link->priv, buf);
		u_server_destroy(link->priv);
		break;

	default:
		break;
	}
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
		u_log(LG_DEBUG, "[%G] -> %s", link, s);
		if (u_msg_parse(&msg, (char*)s) < 0)
			continue;
		u_cmd_invoke(link, &msg, (char*)s);
	}

	/* move remaining buffer contents to the start of the in buffer */
	memmove(link->ibuf, link->ibuf + link->ibuflen - buflen, buflen);
	link->ibuflen = buflen;
}

/* user API */
/* -------- */

void u_link_close(u_link *link)
{
	u_conn_shut_down(link->conn);
}

void u_link_fatal(u_link *link, const char *msg)
{
	exceptional_quit(link, "Fatal error: %s", msg);
	u_link_f(link, "ERROR :%s", msg);

	u_conn_shut_down(link->conn);
}

void u_link_vf(u_link *link, const char *fmt, va_list va)
{
	uchar *buf;
	size_t sz;
	int type;

	if (!link)
		return;

	if (link->auth && link->auth->cls &&
	    link->conn->sendq.size + 512 > link->auth->cls->sendq) {
		on_sendq_full(link->conn);
		return;
	}

	buf = u_conn_get_send_buffer(link->conn, 512);

	if (buf == NULL) {
		on_sendq_full(link->conn);
		return;
	}

	type = FMT_USER;
	if (link->type == LINK_SERVER)
		type = FMT_SERVER;

	sz = vsnf(type, (char*)buf, 510, fmt, va);

	buf[sz] = '\0';

	u_log(LG_DEBUG, "[%G] <- %s", link, buf);

	buf[sz++] = '\r';
	buf[sz++] = '\n';

	u_conn_end_send_buffer(link->conn, sz);
}

void u_link_f(u_link *link, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	u_link_vf(link, fmt, va);
	va_end(va);
}

void u_link_vnum(u_link *link, const char *tgt, int num, va_list va)
{
	char buf[4096];
	char *fmt;

	if (!link)
		return;

	fmt = u_numeric_fmt[num];

	if (fmt == NULL) {
		u_log(LG_SEVERE, "Attempted to use NULL numeric %d", num);
		return;
	}

	/* numerics are ALWAYS FMT_USER */
	vsnf(FMT_USER, buf, 4096, fmt, va);

	u_link_f(link, ":%S %03d %s %s", &me, num, tgt, buf);
}

int u_link_num(u_link *link, int num, ...)
{
	va_list va;

	if (!link)
		return 0;

	va_start(va, num);
	switch (link->type) {
	case LINK_NONE:
		u_link_vnum(link, "*", num, va);
		break;
	case LINK_USER:
		u_user_vnum(link->priv, num, va);
		break;
	default:
		u_log(LG_SEVERE, "Can't use u_link_num on type %d!", link->type);
	}
	va_end(va);

	return 0;
}

/* listeners */
/* --------- */

struct u_link_origin {
	mowgli_eventloop_pollable_t *poll;
	mowgli_node_t n;
};

static mowgli_list_t all_origins;
static mowgli_patricia_t *u_conf_listen_handlers = NULL;

static void accept_ready(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                         mowgli_eventloop_io_dir_t dir, void *priv);

u_link_origin *u_link_origin_create(mowgli_eventloop_t *ev, short port)
{
	const char *operation;
	int opt, fd = -1;
	u_link_origin *origin = NULL;
	struct sockaddr_in addr;

	operation = "create socket";
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		goto error;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	opt = 1;
	operation = "set SO_REUSEADDR";
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		goto error;

	operation = "bind socket";
	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		goto error;

	operation = "listen";
	if (listen(fd, 5) < 0)
		goto error;

	origin = malloc(sizeof(*origin));

	operation = "create pollable";
	if (!(origin->poll = mowgli_pollable_create(ev, fd, origin))) {
		errno = -EINVAL; /* XXX */
		goto error;
	}

	mowgli_node_add(origin, &origin->n, &all_origins);
	mowgli_pollable_setselect(ev, origin->poll, MOWGLI_EVENTLOOP_IO_READ,
	                          accept_ready);

	return origin;

error:
	u_perror(operation);
	if (origin)
		free(origin);
	if (fd >= 0)
		close(fd);
	return NULL;
}

static void accept_ready(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
                         mowgli_eventloop_io_dir_t dir, void *priv)
{
	mowgli_eventloop_pollable_t *poll = mowgli_eventloop_io_pollable(io);
	u_conn *conn;
	u_link *link;

	sync_time();

	link = link_create();

	if (!(conn = u_conn_accept(ev, &u_link_conn_ctx, link, 0, poll->fd))) {
		link_destroy(link);
		/* TODO: close listener, maybe? */
		return;
	}

	u_log(LG_VERBOSE, "new connection from %s", conn->ip);
}

static void *conf_end(void *unused, void *unused2)
{
	if (all_origins.count != 0)
		return NULL;

	u_log(LG_WARN, "No listeners! Opening one on 6667");
	u_link_origin_create(base_ev, 6667);

	return NULL;
}

static void conf_listen(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_conf_traverse(cf, ce->entries, u_conf_listen_handlers);
}

static void conf_listen_port(mowgli_config_file_t *cf,
                             mowgli_config_file_entry_t *ce)
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
		u_link_origin_create(base_ev, low);
	}
}

/* main() API */
/* ---------- */

int init_link(void)
{
	mowgli_list_init(&all_origins);

	u_hook_add(HOOK_CONF_END, conf_end, NULL);
	u_conf_add_handler("listen", conf_listen, NULL);

	u_conf_listen_handlers = mowgli_patricia_create(ascii_canonize);
	u_conf_add_handler("port", conf_listen_port, u_conf_listen_handlers);

	return 0;
}

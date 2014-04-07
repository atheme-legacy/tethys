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
	u_link *link = conn->priv;
	u_link_block *block = link->conf.link;

	u_log(LG_VERBOSE, "link: connect finished");

	u_server_burst_1(link, block);
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

	u_conn_sendq_clear(conn);
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

		/* If executing this command causes an upgrade, u_cmd_invoke will not
		 * return. Indicate the length of the current message to the dump function
		 * so that it won't be serialized and re-execute after upgrade.
		 */
		link->ibufskip = (p - link->ibuf);

		/* dispatch the line */
		u_log(LG_DEBUG, "[%G] -> %s", link, s);
		if (u_msg_parse(&msg, (char*)s) < 0)
			continue;
		u_cmd_invoke(link, &msg, (char*)s);
	}

	/* move remaining buffer contents to the start of the in buffer */
	memmove(link->ibuf, link->ibuf + link->ibuflen - buflen, buflen);
	link->ibuflen = buflen;
	link->ibufskip = 0;
}

void u_link_flush_input(u_link *link) {
	dispatch_lines(link);
}

/* user API */
/* -------- */

u_link *u_link_connect(mowgli_eventloop_t *ev, u_link_block *block,
                       const struct sockaddr *addr, socklen_t addrlen)
{
	u_link *link = link_create();

	u_conn *conn;
	if (!(conn = u_conn_connect(ev, &u_link_conn_ctx, link, 0,
	                            addr, addrlen))) {
		link_destroy(link);
		return NULL;
	}

	link->conf.link = block;

	u_log(LG_VERBOSE, "connecting to %s", conn->ip);

	return link;
}

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

	if (link->sendq > 0 && link->conn->sendq.size + 512 > link->sendq) {
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

u_link_origin *u_link_origin_create_from_fd(mowgli_eventloop_t *ev, int fd)
{
	const char *operation;
	u_link_origin *origin = NULL;

	/* Set the fd to be close-on-exec. All listening sockets will be closed when
	 * we upgrade. This may cause some slight disturbance for users currently
	 * connecting, but this is acceptable.
	 */
	operation = "set close-on-exec";
	if (set_cloexec(fd) < 0)
		goto error;

	u_log(LG_DEBUG, "u_link_origin_create_from_fd: %d", fd);

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
	free(origin);
	return NULL;
}

int u_link_origin_create(mowgli_eventloop_t *ev, ushort port)
{
	int return_code = -1;

	char *host_str = NULL;
	char port_str[8];
	snprintf(port_str, sizeof(port_str), "%hu", port);

	struct addrinfo hints, *res = NULL;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;

	if (getaddrinfo(host_str, port_str, &hints, &res) < 0)
		goto cleanup;

	u_link_origin *origin = NULL;
	struct addrinfo *p;
	int fd, opt = 1;

	for (p = res; p != NULL; p = p->ai_next) {

		if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue;

		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
		if (p->ai_family == PF_INET6)
			setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(int));

		if (bind(fd, p->ai_addr, p->ai_addrlen) < 0) {
			close(fd);
			continue;
		}
		if (listen(fd, 5) < 0) {
			close(fd);
			continue;
		}
		if (! (origin = u_link_origin_create_from_fd(ev, fd))) {
			close(fd);
			continue;
		}

		return_code = 0;

	}

cleanup:
	if (res)
		freeaddrinfo(res);

	return return_code;
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

/* Serialization
 * -------------
 */
mowgli_json_t *u_link_to_json(u_link *link)
{
	mowgli_json_t *jl;

	if (!link)
		return NULL;

	jl = mowgli_json_create_object();
	json_oseti  (jl, "flags", link->flags);
	json_oseti  (jl, "type",  link->type);
	json_osets  (jl, "pass",  link->pass);
	json_oseti  (jl, "sendq", link->sendq);
	json_oseto  (jl, "ck_sendto", u_cookie_to_json(&link->ck_sendto));
	json_oseto  (jl, "conn",  u_conn_to_json(link->conn));
	json_osetb64(jl, "ibuf",  link->ibuf + link->ibufskip, link->ibuflen - link->ibufskip);

	switch (link->type) {
		case LINK_USER:
			/* we can figure this out when we restore */
			break;

		case LINK_SERVER:
			json_osets  (jl, "server_link_block_name", link->conf.link->name);
			break;

		default:
			u_log(LG_SEVERE, "unexpected link type %d", link->type);
			abort();
	}

	return jl;
}

u_link *u_link_from_json(mowgli_json_t *jl)
{
	u_link *link;
	mowgli_json_t *jcookie, *jconn;
	mowgli_string_t *jpass, *jslinkname;
	ssize_t sz;

	link = link_create();

	if (json_ogetu(jl, "flags", &link->flags) < 0)
		goto error;
	if (json_ogetu(jl, "type", &link->type) < 0)
		goto error;
	if (json_ogeti(jl, "sendq", &link->sendq) < 0)
		goto error;

	if ((sz = json_ogetb64(jl, "ibuf", link->ibuf, IBUFSIZE)) < 0)
		goto error;

	link->ibuflen = sz;
	link->ibuf[link->ibuflen] = '\0';

	jpass = json_ogets(jl, "pass");
	if (jpass) {
		link->pass = malloc(jpass->pos+1);
		memcpy(link->pass, jpass->str, jpass->pos);
		link->pass[jpass->pos] = '\0';
	}

	jcookie = json_ogeto(jl, "ck_sendto");
	if (!jcookie)
		goto error;

	if (u_cookie_from_json(jcookie, &link->ck_sendto) < 0)
		goto error;

	jconn = json_ogeto(jl, "conn");
	if (!jconn)
		goto error;

	link->conn = u_conn_from_json(base_ev, &u_link_conn_ctx, link, jconn);
	if (!link->conn)
		goto error;

	link->conn->priv = link;

	/* This must run after the config has been loaded. */
	switch (link->type) {
		case LINK_USER:
			/* If the user is post-registration, re-find his auth block. */
			if (link->flags & U_LINK_REGISTERED) {
				/* XXX: Should this be in user.c? */
				link->conf.auth = u_find_auth(link);
				if (!link->conf.auth)
					goto error;
			}

			break;

		case LINK_SERVER:
			jslinkname = json_ogets(jl, "server_link_block_name");
			if (!jslinkname)
				goto error;

			/* mowgli_string is NULL-terminated. We needn't care about NULLs. */
			link->conf.link = u_find_link(jslinkname->str);
			if (!link->conf.link)
				goto error;

			break;

		default:
			u_log(LG_SEVERE, "unexpected link type %d", link->type);
			abort();
	}

	return link;

error:
	if (link) {
		free(link->pass);
		free(link);
	}
	return NULL;
}

/* vim: set noet: */

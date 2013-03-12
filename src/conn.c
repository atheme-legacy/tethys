#include "ircd.h"

static void origin_recv();

void u_conn_init(conn)
struct u_conn *conn;
{
	conn->flags = 0;
	conn->sock = NULL;
	u_linebuf_init(&conn->ibuf);
	conn->obuf = malloc(U_CONN_OBUFSIZE);
	conn->obuflen = 0;
	conn->obufsize = U_CONN_OBUFSIZE;
	conn->priv = NULL;
	conn->pass = NULL;
	conn->ctx = CTX_UNREG;
}

/* sadfaec */
void u_conn_obufsize(conn, obufsize)
struct u_conn *conn;
int obufsize;
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

void f_str(p, end, s)
char **p, *end, *s;
{
	while (*p < end && *s)
		*(*p)++ = *s++;
}

/* Some day I might write my own string formatter with my own special
   formatting things for use in IRC... but today is not that day */
void u_conn_vf(conn, fmt, va)
struct u_conn *conn;
char *fmt;
va_list va;
{
	char buf[4096];
	char *p, *end;

	p = conn->obuf + conn->obuflen;
	end = conn->obuf + conn->obufsize - 2; /* -2 for \r\n */

	vsprintf(buf, fmt, va);

	f_str(&p, end, buf);

	*p++ = '\r';
	*p++ = '\n';

	conn->obuflen = p - conn->obuf;
}

#ifdef STDARG
void u_conn_f(struct u_conn *conn, char *fmt, ...)
#else
void u_conn_f(conn, fmt, va_alist)
struct u_conn *conn;
char *fmt;
va_dcl
#endif
{
	va_list va;
	va_start(va, fmt);
	u_conn_vf(conn, fmt, va);
	va_end(va);
}

struct u_conn_origin *u_conn_origin_create(io, addr, port, cb)
struct u_io *io;
u_long addr;
u_short port;
void (*cb)();
{
	struct sockaddr_in sa;
	struct u_conn_origin *orig;
	int fd;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		goto out;

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = addr;

	if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0)
		goto out;

	if (listen(fd, 5) < 0)
		goto out;

	/* TODO: setsockopt */

	if (!(orig = malloc(sizeof(*orig))))
		goto out_close;

	if (!(orig->sock = u_io_add_fd(io, fd)))
		goto out_free;

	orig->cb = cb;
	orig->sock->recv = origin_recv;
	orig->sock->send = NULL;
	orig->sock->priv = orig;

	return orig;

out_free:
	free(orig);
out_close:
	close(fd);
out:
	return NULL;
}

static void origin_recv(sock)
struct u_io_fd *sock;
{
	struct u_conn_origin *orig = sock->priv;
	struct u_io_fd *iofd;
	struct sockaddr addr;
	int addrlen;
	int fd;

	if ((fd = accept(sock->fd, &addr, &addrlen)) < 0)
		return;

	if (!(iofd = u_io_add_fd(sock->io, fd)))
		return; /* XXX */

	iofd->priv = NULL;
	iofd->recv = NULL;
	iofd->send = NULL;
	orig->cb(iofd);
}

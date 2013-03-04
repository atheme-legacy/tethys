#include "ircd.h"

static void origin_recv();

void u_connection_init(conn)
struct u_connection *conn;
{
	conn->flags = 0;
	conn->sock = NULL;
	u_linebuf_init(&conn->ibuf);
	conn->obuf = malloc(U_CONNECTION_OBUFSIZE);
	conn->obuflen = 0;
	conn->obufsize = U_CONNECTION_OBUFSIZE;
}

/* sadfaec */
void u_connection_obufsize(conn, obufsize)
struct u_connection *conn;
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
	sa.sin_port = port;
	sa.sin_addr.s_addr = addr;

	if (bind(fd, &sa, sizeof(sa)) < 0)
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

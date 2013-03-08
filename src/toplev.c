#include "ircd.h"

static void toplev_sync();

void u_toplev_origin_cb(iofd)
struct u_io_fd *iofd;
{
	struct u_conn *conn;

	conn = malloc(sizeof(*conn));
	u_conn_init(conn);
	conn->sock = iofd;

	iofd->priv = conn;

	toplev_sync(iofd);
}

static void toplev_recv(conn)
struct u_conn *conn;
{
	char buf[1024];
	int sz;
	struct u_msg msg;

	sz = recv(conn->sock->fd, buf, 1024-conn->ibuf.pos, 0);
	u_linebuf_data(&conn->ibuf, buf, sz);

	while ((sz = u_linebuf_line(&conn->ibuf, buf, 1024)) != 0) {
		if (sz < 0) /* TODO: error */
			break;
		buf[sz] = '\0';
		if (strlen(buf) != sz) /* TODO: error */
			break;
		u_msg_parse(&msg, buf);
		conn->invoke(conn, &msg);
	}
}

static void toplev_send(iofd)
struct u_io_fd *iofd;
{
	/* TODO: toplev send */
}

static void toplev_sync(iofd)
struct u_io_fd *iofd;
{
	struct u_conn *conn = iofd->priv;
	iofd->recv = iofd->send = NULL;
	if (!(conn->flags & U_CONN_CLOSING))
		iofd->recv = toplev_recv;
	if (conn->obuflen > 0)
		iofd->send = toplev_send;
}

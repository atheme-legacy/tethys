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

static void toplev_recv(iofd)
struct u_io_fd *iofd;
{
	struct u_conn *conn = iofd->priv;
	char buf[1024];
	int sz;
	struct u_msg msg;

	sz = recv(iofd->fd, buf, 1024-conn->ibuf.pos, 0);

	if (sz <= 0) {
		perror("toplev_recv");
		iofd->recv = NULL;
		return;
	}

	u_linebuf_data(&conn->ibuf, buf, sz);

	while ((sz = u_linebuf_line(&conn->ibuf, buf, 1024)) != 0) {
		if (sz < 0) /* TODO: error */
			break;
		buf[sz] = '\0';
		if (strlen(buf) != sz) /* TODO: error */
			break;
		u_msg_parse(&msg, buf);
		u_cmd_invoke(conn, &msg);
	}

	toplev_sync(iofd);
}

static void toplev_send(iofd)
struct u_io_fd *iofd;
{
	struct u_conn *conn = iofd->priv;
	int sz;

	sz = send(iofd->fd, conn->obuf, conn->obuflen, 0);

	if (sz < 0) {
		perror("toplev_send");
		iofd->send = NULL;
		return;
	}

	if (sz > 0) {
		u_memmove(conn->obuf, conn->obuf + sz, conn->obufsize - sz);
		conn->obuflen -= sz;
	}

	toplev_sync(iofd);
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

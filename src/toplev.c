#include "ircd.h"

static void toplev_sync();

void u_toplev_origin_cb(iofd)
struct u_io_fd *iofd;
{
	toplev_sync(iofd);
}

void toplev_cleanup(iofd)
struct u_io_fd *iofd;
{
	struct u_conn *conn = iofd->priv;
	u_conn_event(conn, EV_DESTROYING);
	u_conn_cleanup(conn);
	close(iofd->fd);
	free(conn);
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
		u_conn_event(conn, sz == 0 ? EV_END_OF_STREAM : EV_RECV_ERROR);
		u_conn_close(conn);
		toplev_sync(iofd);
		return;
	}

	if (u_linebuf_data(&conn->ibuf, buf, sz) < 0) {
		u_conn_event(conn, EV_RECVQ_FULL);
		u_conn_close(conn);
		toplev_sync(iofd);
		return;
	}

	while ((sz = u_linebuf_line(&conn->ibuf, buf, 1024)) != 0) {
		if (sz > 0)
			buf[sz] = '\0';
		if (sz < 0 || strlen(buf) != sz) {
			u_conn_event(conn, EV_RECV_ERROR);
			u_conn_close(conn);
			break;
		}
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
		u_conn_event(conn, EV_SEND_ERROR);
		u_conn_close(conn);
		toplev_sync(iofd);
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
	if (!iofd->recv && !iofd->send)
		toplev_cleanup(iofd);
}

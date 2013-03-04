#include "ircd.h"

static void toplev_sync_c();
static void toplev_sync_u();
static void toplev_sync_s();

void u_toplev_origin_cb(iofd)
struct u_io_fd *iofd;
{
	struct u_connection *conn;

	conn = malloc(sizeof(*conn));
	u_connection_init(conn);
	conn->sock = iofd;

	iofd->priv = conn;

	toplev_sync_c(iofd);
}

static int get_lines(conn)
struct u_connection *conn;
{
	char buf[U_LINEBUF_SIZE];
	int sz;

	sz = recv(conn->sock->fd, buf, U_LINEBUF_SIZE-conn->ibuf.pos, 0);
	u_linebuf_data(&conn->ibuf, buf, sz);
}

static void do_recv(conn, data, invoke)
struct u_connection *conn;
void *data;
void (*invoke)();
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
		invoke(data, &msg);
	}
}

static void toplev_recv_c(iofd)
struct u_io_fd *iofd;
{
	do_recv(iofd->priv, iofd->priv, u_cmd_invoke_c);
}

static void toplev_recv_u(iofd)
struct u_io_fd *iofd;
{
	struct u_user_local *user = iofd->priv;
	do_recv(user->conn, user, u_cmd_invoke_u);
}

static void toplev_recv_s(iofd)
struct u_io_fd *iofd;
{
	struct u_server *serv = iofd->priv;
	do_recv(serv->conn, serv, u_cmd_invoke_s);
}

static void toplev_send(iofd)
struct u_io_fd *iofd;
{
	/* TODO: toplev send */
}

static void toplev_sync(iofd, conn, recvfn)
struct u_io_fd *iofd;
struct u_connection *conn;
void (*recvfn)();
{
	iofd->recv = iofd->send = NULL;
	if (!(conn->flags & U_CONNECTION_CLOSING))
		iofd->recv = recvfn;
	if (conn->obuflen > 0)
		iofd->send = toplev_send;
}

static void toplev_sync_c(iofd)
struct u_io_fd *iofd;
{
	struct u_connection *conn = iofd->priv;
	toplev_sync(iofd, conn, toplev_recv_c);
}

static void toplev_sync_u(iofd)
struct u_io_fd *iofd;
{
	struct u_user_local *user = iofd->priv;
	toplev_sync(iofd, user->conn, toplev_recv_u);
}

static void toplev_sync_s(iofd)
struct u_io_fd *iofd;
{
	struct u_server *serv = iofd->priv;
	toplev_sync(iofd, serv->conn, toplev_recv_s);
}

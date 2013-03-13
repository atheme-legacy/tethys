#ifndef __INC_CONN_H__
#define __INC_CONN_H__

#define U_CONN_OBUFSIZE 2048

#define U_CONN_CLOSING 0x0001

/* events */
#define EV_END_OF_STREAM  1
#define EV_RECV_ERROR     2
#define EV_SEND_ERROR     3
#define EV_SENDQ_FULL     4
#define EV_RECVQ_FULL     5
#define EV_DESTROYING     6

/* connection contexts. used for command processing */
#define CTX_UNREG       0
#define CTX_USER        1
#define CTX_SERVER      2
#define CTX_UREG        3
#define CTX_SREG        4
#define CTX_MAX         5

struct u_conn {
	unsigned flags;
	struct u_io_fd *sock;
	struct u_linebuf ibuf;
	char ip[INET_ADDRSTRLEN];
	char *obuf;
	int obuflen, obufsize;
	void (*event)(); /* u_conn*, int event */
	void *priv;
	char *pass;
	int ctx;
};

struct u_conn_origin {
	struct u_io_fd *sock;
	void (*cb)(); /* u_io_fd* */
};

extern void u_conn_init(); /* u_conn* */
extern void u_conn_cleanup(); /* u_conn* */
extern void u_conn_obufsize(); /* u_conn*, int obufsize */

/* deletes all but up to one line of output */
extern void u_conn_out_clear(); /* u_conn* */

extern void u_conn_vf(); /* u_conn*, char *fmt, va_list */
extern void u_conn_f(A(struct u_conn *conn, char *fmt, ...));

extern void u_conn_event(); /* u_conn*, int */
extern void u_conn_close(); /* u_conn* */

/* u_io*, u_long addr, u_short port, void (*cb)(); */
extern struct u_conn_origin *u_conn_origin_create();

#endif

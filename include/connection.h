#ifndef __INC_CONNECTION_H__
#define __INC_CONNECTION_H__

#define U_CONNECTION_OBUFSIZE 2048

struct u_connection {
	struct u_io_fd *sock;
	struct u_linebuf ibuf;
	char *obuf;
	int obuflen, obufsize;
};

struct u_conn_origin {
	struct u_io_fd *sock;
	void (*cb)(); /* u_io_fd* */
};

extern void u_connection_init(); /* u_connection* */
extern void u_connection_obufsize(); /* u_connection*, int obufsize */

/* u_io*, u_long addr, u_short port, void (*cb)(); */
extern struct u_conn_origin *u_conn_origin_create();

#endif

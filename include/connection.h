#ifndef __INC_CONNECTION_H__
#define __INC_CONNECTION_H__

struct u_connection {
	struct u_io_fd *sock;
	struct u_linebuf ibuf;
	char *obuf;
	int obuflen, obufsize;
};

extern void u_connection_init(); /* u_connection*, int obufsize */

#endif

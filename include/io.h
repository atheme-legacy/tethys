#ifndef __INC_IO_H__
#define __INC_IO_H__

struct u_io {
	int running;
	struct u_list fds;
	struct u_list timers;
};

struct u_io_fd {
	struct u_io *io;
	struct u_list *n;
	int fd;
	void *priv;
	/* both accept struct u_io_fd as the sole arg. if both are NULL,
	   the iofd is deleted */
	void (*recv)();
	void (*send)();
};

struct u_io_timer {
	struct u_io *io;
	struct u_list *n;
	/* accepts struct u_io_timer as sole arg */
	long time;
	void (*fire)();
};

extern long NOW;

extern void u_io_init(); /* struct u_io* */
extern struct u_io_fd *u_io_add_fd(); /* struct u_io*, int */
extern struct u_io_timer *u_io_add_timer(); /* struct u_io*, long */
extern void u_io_del_timer(); /* u_io*, u_io_timer* */

extern void u_io_poll_once(); /* struct u_io* */
extern void u_io_poll_break(); /* struct u_io* */
extern void u_io_poll(); /* struct u_io* */

#endif

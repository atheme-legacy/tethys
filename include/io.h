/* ircd-micro, io.h -- async IO loop
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

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
	struct timeval time;
	/* u_io_timer* */
	void (*cb)();
	void *priv;
};

extern struct timeval NOW;

extern void u_io_init(); /* struct u_io* */
extern struct u_io_fd *u_io_add_fd(); /* struct u_io*, int */
extern struct u_io_timer *u_io_add_timer(); /* struct u_io*, sec, usec, cb, priv */
extern void u_io_del_timer(); /* u_io*, u_io_timer* */

extern void u_io_poll_once(); /* struct u_io* */
extern void u_io_poll_break(); /* struct u_io* */
extern void u_io_poll(); /* struct u_io* */

#endif

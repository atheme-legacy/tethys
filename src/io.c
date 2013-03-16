/* ircd-micro, io.c -- async IO loop
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

long NOW = 0;

void u_io_init(io)
struct u_io *io;
{
	io->running = 0;
	u_list_init(&io->fds);
	u_list_init(&io->timers);
}

struct u_io_fd *u_io_add_fd(io, fd)
struct u_io *io;
int fd;
{
	struct u_io_fd *iofd;

	iofd = (struct u_io_fd*)malloc(sizeof(*iofd));
	iofd->io = io;
	iofd->fd = fd;

	iofd->n = u_list_add(&io->fds, iofd);
	if (iofd->n == NULL) {
		free(iofd);
		return NULL;
	}

	u_log(LG_DEBUG, "IO: +++ FD=%3d [%p]", iofd->fd, iofd);

	return iofd;
}

void u_io_del_fd(io, iofd)
struct u_io *io;
struct u_io_fd *iofd;
{
	u_log(LG_DEBUG, "IO: --- FD=%3d [%p]", iofd->fd, iofd);
	u_list_del_n(iofd->n);
}

struct u_io_timer *u_io_add_timer(io, time)
struct u_io *io;
long time;
{
	struct u_io_timer *iot;

	iot = (struct u_io_timer*)malloc(sizeof(*iot));
	iot->time = time;

	iot->n = u_list_add(&io->timers, iot);
	if (iot->n == NULL) {
		free(iot);
		return NULL;
	}

	iot->io = io;

	return iot;
}

void u_io_del_timer(io, iot)
struct u_io *io;
struct u_io_timer *iot;
{
	u_list_del_n(iot->n);
}

void u_io_poll_once(io)
struct u_io *io;
{
	fd_set r, w;
	int nfds;
	long next;
	struct u_list *n, *tn;
	struct u_io_timer *iot;
	struct u_io_fd *iofd;
	struct timeval _tv, *tv;

	FD_ZERO(&r);
	FD_ZERO(&w);

	NOW = time(0);

	next = -1;
	U_LIST_EACH(n, &io->timers) {
		iot = n->data;
		if (iot->time <= NOW) {
			next = NOW;
			break;
		}
		if (next == -1 || iot->time < next)
			next = iot->time;
	}

	nfds = 0;
	U_LIST_EACH(n, &io->fds) {
		iofd = n->data;
		if (iofd->fd > nfds)
			nfds = iofd->fd;
		if (iofd->recv)
			FD_SET(iofd->fd, &r);
		if (iofd->send)
			FD_SET(iofd->fd, &w);
	}

	_tv.tv_sec = next - NOW;
	_tv.tv_usec = 0;
	tv = (next == -1) ? NULL : &_tv;

	nfds = select(nfds+1, &r, &w, NULL, tv);

	NOW = time(0);

	U_LIST_EACH_SAFE(n, tn, &io->timers) {
		iot = n->data;
		if (iot->time <= NOW) {
			iot->fire(iot);
			u_io_del_timer(io, iot);
		}
	}

	U_LIST_EACH_SAFE(n, tn, &io->fds) {
		iofd = n->data;
		if (FD_ISSET(iofd->fd, &r) && iofd->recv)
			iofd->recv(iofd);
		if (FD_ISSET(iofd->fd, &w) && iofd->send)
			iofd->send(iofd);
		if (!iofd->recv && !iofd->send)
			u_io_del_fd(io, iofd);
	}
}

void u_io_poll_break(io)
struct u_io *io;
{
	io->running = 0;
}

void u_io_poll(io)
struct u_io *io;
{
	io->running = 1;

	while (io->running) {
		u_io_poll_once(io);
	}
}

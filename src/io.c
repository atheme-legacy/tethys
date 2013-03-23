/* ircd-micro, io.c -- async IO loop
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* I got tired of writing this out all the time */
#define MILLION 1000000

struct timeval NOW;

void tv_cpy(res, a) /* res = a */
struct timeval *res, *a;
{
	res->tv_sec = a->tv_sec;
	res->tv_usec = a->tv_usec;
}

void tv_norm(tv)
struct timeval *tv;
{
	if (tv->tv_usec >= MILLION) {
		tv->tv_sec += (int)(tv->tv_usec / MILLION);
		tv->tv_usec %= MILLION;
	} else if (tv->tv_usec < 0) {
		/* XXX XXX XXX */
		while (tv->tv_usec < 0) {
			tv->tv_usec += MILLION;
			tv->tv_sec --;
		}
	}
}

void tv_add(a, b, res) /* res = a + b */
struct timeval *a, *b, *res;
{
	res->tv_sec = a->tv_sec + b->tv_sec;
	res->tv_usec = a->tv_usec + b->tv_sec;
	if (res->tv_usec >= 1000000) {
		res->tv_usec -= 1000000;
		res->tv_sec ++;
	}
}

void tv_sub(a, b, res) /* res = a - b */
struct timeval *a, *b, *res;
{
	res->tv_sec = a->tv_sec - b->tv_sec;
	res->tv_usec = a->tv_usec - b->tv_usec;
	if (res->tv_usec < 0) {
		res->tv_usec += 1000000;
		res->tv_sec --;
	}
}

void tv_clear(tv)
struct timeval *tv;
{
	tv->tv_sec = 0;
	tv->tv_usec = 0;
}

int tv_isset(tv)
struct timeval *tv;
{
	return tv->tv_sec != 0 || tv->tv_usec != 0;
}

int tv_cmp(a, b)
struct timeval *a, *b;
{
	long x, y;
	x = a->tv_sec;
	y = b->tv_sec;
	if (x == y) {
		x = a->tv_usec;
		y = b->tv_usec;
	}
	if (x == y)
		return 0;
	return x < y ? -1 : 1;
}

void u_io_init(io)
struct u_io *io;
{
	io->running = 0;
	u_list_init(&io->fds);
	u_list_init(&io->timers);
	gettimeofday(&NOW, NULL);
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

	u_log(LG_FINE, "IO: +++ FD=%3d [%p]", iofd->fd, iofd);

	return iofd;
}

void u_io_del_fd(io, iofd)
struct u_io *io;
struct u_io_fd *iofd;
{
	u_log(LG_FINE, "IO: --- FD=%3d [%p]", iofd->fd, iofd);
	u_list_del_n(iofd->n);
}

struct u_io_timer *u_io_add_timer(io, sec, usec, cb, priv)
struct u_io *io;
unsigned long sec, usec;
void (*cb)();
void *priv;
{
	struct u_io_timer *iot;

	iot = (struct u_io_timer*)malloc(sizeof(*iot));

	iot->n = u_list_add(&io->timers, iot);
	if (iot->n == NULL) {
		free(iot);
		return NULL;
	}

	iot->io = io;
	iot->cb = cb;
	iot->priv = priv;
	iot->time.tv_sec = NOW.tv_sec + sec;
	iot->time.tv_usec = NOW.tv_usec + usec;
	tv_norm(&iot->time);

	return iot;
}

void u_io_del_timer(iot)
struct u_io_timer *iot;
{
	u_list_del_n(iot->n);
	free(iot);
}

void u_io_poll_once(io)
struct u_io *io;
{
	fd_set r, w;
	int nfds;
	struct u_list *n, *tn;
	struct u_io_timer *iot;
	struct u_io_fd *iofd;
	struct timeval tv, *tvp;

	FD_ZERO(&r);
	FD_ZERO(&w);

	gettimeofday(&NOW, NULL);

	tv_clear(&tv);
	U_LIST_EACH(n, &io->timers) {
		iot = n->data;
		if (tv_cmp(&iot->time, &NOW) <= 0) {
			tv_cpy(&tv, &NOW);
			break;
		}
		if (!tv_isset(&tv) || tv_cmp(&iot->time, &tv) < 0)
			tv_cpy(&tv, &iot->time);
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

	tvp = NULL;
	if (tv_isset(&tv)) {
		tv_sub(&tv, &NOW, &tv);
		tvp = &tv;
	}

	nfds = select(nfds+1, &r, &w, NULL, tvp);

	gettimeofday(&NOW, NULL);

	U_LIST_EACH_SAFE(n, tn, &io->timers) {
		iot = n->data;
		if (tv_cmp(&iot->time, &NOW) < 0) {
			iot->cb(iot);
			u_io_del_timer(iot);
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

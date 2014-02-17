/* Tethys, sendq.c -- dynamic send queues
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* chunks */
/* ------ */

#define SENDQ_CHUNK_SIZE 4000

#define CHUNK_IN_USE 0x0001

struct u_sendq_chunk {
	uchar data[SENDQ_CHUNK_SIZE];
	ulong flags;
	int start, end;
	u_sendq_chunk *next;
};

#define SENDQ_CHUNK_BACKLOG_MAX 400

static u_sendq_chunk *free_chunks = NULL;
static int num_free_chunks = 0;

static u_sendq_chunk *chunk_new(void)
{
	u_sendq_chunk *chunk;

	if (num_free_chunks) {
		num_free_chunks--;
		chunk = free_chunks;
		free_chunks = chunk->next;
	} else {
		u_log(LG_DEBUG, "sendq chunk: malloc()");
		chunk = malloc(sizeof(*chunk));
	}

	chunk->flags = CHUNK_IN_USE;
	chunk->next = NULL;
	chunk->start = chunk->end = 0;
	return chunk;
}

static void chunk_free(u_sendq_chunk *chunk)
{
	if (!(chunk->flags & CHUNK_IN_USE)) /* prevent multiple free */
		return;

	if (num_free_chunks >= SENDQ_CHUNK_BACKLOG_MAX) {
		u_log(LG_DEBUG, "sendq chunk: free()");
		free(chunk);
		return;
	}

	chunk->flags &= ~CHUNK_IN_USE;
	chunk->next = free_chunks;
	free_chunks = chunk;
	num_free_chunks ++;
}

/* create, destroy */
/* --------------- */

void u_sendq_init(u_sendq *q)
{
	memset(q, 0, sizeof(*q));
}

void u_sendq_clear(u_sendq *q)
{
	u_sendq_chunk *ch;

	for (ch = q->head; ch; ch = ch->next)
		chunk_free(ch);

	memset(q, 0, sizeof(*q));
}

/* buffer interaction */
/* ------------------ */

uchar *u_sendq_get_buffer(u_sendq *q, size_t sz)
{
	u_sendq_chunk *chunk;

	if (sz > SENDQ_CHUNK_SIZE) {
		/* TODO: this is not exactly robust */
		return NULL;
	}

	chunk = q->tail;

	if (!chunk || sz > (SENDQ_CHUNK_SIZE - chunk->end)) {
		chunk = chunk_new();

		if (q->tail != NULL)
			q->tail->next = chunk;

		if (q->head == NULL)
			q->head = chunk;

		q->tail = chunk;
	}

	return chunk->data + chunk->end;
}

size_t u_sendq_end_buffer(u_sendq *q, size_t sz)
{
	u_sendq_chunk *chunk = q->tail;

	if (!chunk) {
		u_log(LG_WARN, "sendq: potential heap corruption!");
		return 0;
	}

	if (chunk->end + sz > SENDQ_CHUNK_SIZE) {
		u_log(LG_WARN, "sendq: potential heap corruption!");
		sz = SENDQ_CHUNK_SIZE - chunk->end;
	}

	chunk->end += sz;
	q->size += sz;

	return sz;
}

#define NUM_IOVECS 32

int u_sendq_write(u_sendq *q, int fd)
{
	u_sendq_chunk *ch = q->head;
	struct iovec iov[NUM_IOVECS];
	int iovcnt = 0;
	ssize_t sz;

	for (; iovcnt < NUM_IOVECS && ch; iovcnt++, ch = ch->next) {
		iov[iovcnt].iov_base = ch->data + ch->start;
		iov[iovcnt].iov_len = ch->end - ch->start;
		u_log(LG_DEBUG, "ch %p %04d-%04d -> iov %p +%04u",
		      ch->data, ch->start, ch->end,
		      iov[iovcnt].iov_base, iov[iovcnt].iov_len);
	}

	sz = writev(fd, iov, iovcnt);

	if (sz < 0)
		return sz;

	q->size -= sz;

	for (ch = q->head; ch; ch = ch->next) {
		int chsz = ch->end - ch->start;

		if (chsz > sz) {
			/* didn't send all data in this chunk */
			ch->start += sz;
			break;
		}

		sz -= chsz;

		q->head = ch->next;
		if (q->tail == ch)
			q->tail = NULL;
		chunk_free(ch);
	}

	return 0;
}

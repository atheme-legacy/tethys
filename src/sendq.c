/* Tethys, sendq.c -- dynamic send queues
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* chunks */
/* ------ */

#define SENDQ_CHUNK_SIZE 4000
#define SENDQ_B64_CHUNK_SIZE 5328 /* corresponds to just under 4000 bytes */

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
	u_sendq_chunk *ch, *nch;

	for (ch = q->head; ch; ch = nch) {
		nch = ch->next;
		chunk_free(ch);
	}

	memset(q, 0, sizeof(*q));
}

/* buffer interaction */
/* ------------------ */

static u_sendq_chunk *sendq_append_chunk(u_sendq *q)
{
	u_sendq_chunk *chunk;

	chunk = chunk_new();

	if (q->tail != NULL)
		q->tail->next = chunk;

	if (q->head == NULL)
		q->head = chunk;

	q->tail = chunk;

	return chunk;
}

static void sendq_delete_chunk(u_sendq *q, u_sendq_chunk *chunk)
{
	if (q->head != chunk)
		abort();

	q->head = chunk->next;

	if (q->head == NULL)
		q->tail = NULL;

	chunk_free(chunk);
}

uchar *u_sendq_get_buffer(u_sendq *q, size_t sz)
{
	u_sendq_chunk *chunk;

	if (sz > SENDQ_CHUNK_SIZE) {
		/* TODO: this is not exactly robust */
		return NULL;
	}

	chunk = q->tail;

	if (!chunk || sz > (SENDQ_CHUNK_SIZE - chunk->end))
		chunk = sendq_append_chunk(q);

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
		u_log(LG_FINE, "  sendq: ch %p %04d-%04d -> iov %p +%04u",
		      ch->data, ch->start, ch->end,
		      iov[iovcnt].iov_base, iov[iovcnt].iov_len);
	}

	sz = writev(fd, iov, iovcnt);

	if (sz < 0)
		return sz;

	q->size -= sz;

	while ((ch = q->head) != NULL) {
		int chsz = ch->end - ch->start;

		if (chsz > sz) {
			/* didn't send all data in this chunk */
			ch->start += sz;
			break;
		}

		sz -= chsz;

		sendq_delete_chunk(q, ch);
	}

	return 0;
}

/* Serialization
 * -------------
 */
mowgli_json_t *u_sendq_to_json(u_sendq *sq)
{
	mowgli_json_t *jsq, *jbuf;
	u_sendq_chunk *c;
	char *ocur;
	char *sqbuf;
	size_t sqbuf_len;

	if (!sq)
		return NULL;

	/* Might want to optimize this in the future. */
	sqbuf_len = base64_inflate_size(sq->size);
	sqbuf = malloc(sqbuf_len);

	ocur = sqbuf;
	for (c=sq->head; c; c=c->next) {
		/* we can assume the chunk is in use */
		ocur += base64_encode(c->data + c->start, c->end - c->start,
			sqbuf, ocur);
		/* invariant: ocur <= sqbuf + sqbuf_len */
	}

	jsq  = mowgli_json_create_object();
	jbuf = mowgli_json_create_string_n(sqbuf, ocur - sqbuf);
	json_oseto(jsq, "buf", jbuf);

	free(sqbuf);
	return jsq;
}

int u_sendq_from_json(mowgli_json_t *jsq, u_sendq *sq)
{
	mowgli_string_t *jsbuf;
	size_t len;
	char *cur, *end, *echunk;
	void *sqbuf;

	jsbuf = json_ogets(jsq, "buf");
	if (!jsbuf || jsbuf->pos % 4)
		return -1;

	cur = jsbuf->str;
	end = jsbuf->str + jsbuf->pos;

	/* base64 data length must be divisible by 4. */
	if (jsbuf->pos % 4)
		return -1;

	/* We choose to decode in blocks of 5328 characters, as this corresponds to
	 * just under 4000 bytes and is divisible by 4.
	 */
	while (cur < end) {
		echunk = cur + SENDQ_B64_CHUNK_SIZE;
		if (echunk > end)
			echunk = end;

		sqbuf = u_sendq_get_buffer(sq, SENDQ_CHUNK_SIZE);
		len = base64_decode(cur, echunk-cur, sqbuf);
		u_sendq_end_buffer(sq, len);

		cur = echunk;
	}

	return 0;
}

/* vim: set noet: */

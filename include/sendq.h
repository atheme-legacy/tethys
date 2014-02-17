/* Tethys, sendq.h -- dynamic send queues
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_SENDQ_H__
#define __INC_SENDQ_H__

typedef struct u_sendq u_sendq;
typedef struct u_sendq_chunk u_sendq_chunk;

struct u_sendq {
	size_t size;
	u_sendq_chunk *head, *tail;
};

extern void u_sendq_init(u_sendq*);
extern void u_sendq_clear(u_sendq*);

/* TODO: extern void u_sendq_put(u_sendq*, uchar*, size_t); */

/* to allow vsnf, sprintf, etc. directly into the send queue */
extern uchar *u_sendq_get_buffer(u_sendq*, size_t sz);
extern size_t u_sendq_end_buffer(u_sendq*, size_t sz);

extern int u_sendq_write(u_sendq*, int fd);

#endif

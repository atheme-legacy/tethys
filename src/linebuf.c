/* ircd-micro, linebuf.c -- line buffering
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void u_linebuf_init(lb)
u_linebuf *lb;
{
	lb->pos = 0;
}

int u_linebuf_data(lb, data, size)
u_linebuf *lb;
char *data;
int size;
{
	if (lb->pos + size > U_LINEBUF_SIZE)
		return -1; /* linebuf full! */
	memcpy(lb->buf + lb->pos, data, size);
	lb->pos += size;
	return 0;
}

int u_linebuf_line(lb, dest, size)
u_linebuf *lb;
char *dest;
int size;
{
	/* s and p are just generic pointers whose meaning changes throughout
	   the function. */
	char *s, *p;
	int sz;

	s = (char*)memchr(lb->buf, '\n', lb->pos);
	p = (char*)memchr(lb->buf, '\r', lb->pos);

	if (!s && !p)
		return 0;

	if (!s || (p && p < s))
		s = p;

	p = s;
	while (p - lb->buf < lb->pos && (*p == '\n' || *p == '\r'))
		p++;

	if (s - lb->buf > size)
		return -1;

	sz = s - lb->buf;
	memcpy(dest, lb->buf, sz);

	u_memmove(lb->buf, p, lb->buf - p + U_LINEBUF_SIZE);
	lb->pos -= p - lb->buf;

	return sz;
}

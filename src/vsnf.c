/* ircd-micro, vsnf.c -- string formatter
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

struct buffer {
	char *p, *base;
	uint len, size;
};

void string(buf, s, copylen) struct buffer *buf; char *s;
{
	if (s == NULL) {
		string(buf, "(null)", 6);
		return;
	}

	if (copylen < 0) /* may be needed for shenanigans later */
		copylen = strlen(s);

	if (copylen > (buf->size - buf->len))
		copylen = buf->size - buf->len;

	memcpy(buf->p, s, copylen);
	buf->p += copylen;
	buf->len += copylen;
}

void character(buf, c) struct buffer *buf; char c;
{
	if (buf->len > buf->size)
		return;
	*(buf->p++) = c;
	buf->len++;
}

struct intopts {
	uint n, base, sign;

	/* these fields used for %p: */
	int width;
	char pad;
};

void integer(buf, n) struct buffer *buf; struct intopts *n;
{
	static char *digits = "0123456789abcdef";
	int negative = 0;
	char *s, buf2[64];

	s = buf2 + 64;
	*--s = '\0';

	if (n->sign && (int)n->n < 0) {
		negative = 1;
		n->n = -n->n;
	}

	if (n->n == 0) /* special case? */
		*--s = digits[0];

	while (n->n > 0 || n->width > 0) {
		n->width--;

		if (n->n == 0) {
			*--s = n->pad;
			continue;
		}

		*--s = digits[n->n % n->base];
		n->n /= n->base;

		if (n->n == 0 && negative)
			*--s = '-';
	}

	string(buf, s, buf2 + 63 - s);
}

void quick_uint(buf, prefix, n, base)
struct buffer *buf; char *prefix; uint n, base;
{
	struct intopts opts;

	opts.n = n;
	opts.base = base;
	opts.sign = 0;
	opts.width = 0;
	opts.pad = '0';

	if (prefix != NULL)
		string(buf, prefix, -1);
	integer(buf, &opts);
}

int vsnf(type, s, size, fmt, va) char *s, *fmt; uint size; va_list va;
{
	char c_arg, *s_arg;
	u_user *user;
	u_server *server;

	struct buffer buf;
	struct intopts n;
	int debug = 0;

	if (type == FMT_DEBUG) {
		debug = 1;
		type = FMT_LOG;
	}

	buf.p = buf.base = s;
	buf.size = size - 1; /* null byte */
	buf.len = 0;

	/* a goto is used to reduce indentation */
top:
	if (!*fmt)
		goto bottom;

	if (*fmt != '%') {
		character(&buf, *fmt++);
		goto top;
	}

	n.base = 0;
	n.sign = 0;
	n.width = 0;
	n.pad = '0';

	switch (*++fmt) {
	/* useful IRC formats */
	case 'T': /* timestamp */
		if (type == FMT_LOG) {
			character(&buf, '(');
			string(&buf, ctime(&NOW.tv_sec), 24);
			character(&buf, ')');
		} else {
			quick_uint(&buf, NULL, NOW.tv_sec, 10);
		}
		break;

	case 'U': /* user */
		user = va_arg(va, u_user*);
		if (type == FMT_SERVER) {
			string(&buf, user->uid, 9);
		} else {
			string(&buf, user->nick, -1);
			if (debug) {
				quick_uint(&buf, "[0x", user, 16);
				character(&buf, ']');
			}
		}
		break;

	case 'H': /* hostmask */
		user = va_arg(va, u_user*);
		if (type == FMT_SERVER) {
			string(&buf, user->uid, 9);
		} else {
			string(&buf, user->nick, -1);
			character(&buf, '!');
			string(&buf, user->ident, -1);
			character(&buf, '@');
			string(&buf, user->host, -1);
			if (debug) {
				quick_uint(&buf, "[0x", user, 16);
				character(&buf, ']');
			}
		}
		break;

	case 'S': /* server */
		server = va_arg(va, u_server*);
		if (type == FMT_SERVER) {
			string(&buf, server->sid, 3);
		} else {
			string(&buf, server->name, -1);
			if (debug) {
				quick_uint(&buf, "[0x", user, 16);
				character(&buf, ']');
			}
		}
		break;

	/* standard printf-family formats */
	case 's':
		s_arg = va_arg(va, char*);
		string(&buf, s_arg, -1);
		break;

	case 'd':
		n.sign = 1;
	case 'u':
		n.base = 10;
	case 'o':
	case 'x':
	case 'p':
		n.n = va_arg(va, uint);
		if (n.base == 0) /* eww, further hax */
			n.base = (*fmt == 'o') ? 8 : 16;

		if (*fmt == 'p') {
			n.width = 8;
			string(&buf, "0x", 2);
		}

		integer(&buf, &n);
		break;

	case 'c':
		c_arg = va_arg(va, int);
		character(&buf, c_arg);
		break;

	default:
		/* print a warning? */
		character(&buf, '%');
	case '%':
		character(&buf, *fmt);
	}

	fmt++;
	goto top;
bottom:

	*(buf.p) = '\0';
	return buf.len;
}

#ifdef STDARG
int snf(int type, char *s, uint size, char *fmt, ...)
#else
int snf(type, s, size, fmt, va_alist) char *s, *fmt; uint size; va_dcl
#endif
{
	va_list va;
	int ret;
	u_va_start(va, fmt);
	ret = vsnf(type, s, size, fmt, va);
	va_end(va);
	return ret;
}

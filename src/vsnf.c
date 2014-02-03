/* Tethys, vsnf.c -- string formatter
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

#undef VSNF_LOG

struct buffer {
	char *p, *base;
	uint len, size;
};

struct spec {
	int width;
	char pad;
};
static struct spec __spec_default = { 0, ' ' };

static void string(struct buffer *buf, char *s, int copylen, struct spec *spec)
{
	int width;

	if (s == NULL) {
		string(buf, "(null)", 6, spec);
		return;
	}

	if (spec == NULL)
		spec = &__spec_default;
	width = spec->width;

	if (copylen < 0)
		copylen = strlen(s);

	if (width > copylen) {
		if (width - copylen > buf->size - buf->len)
			width = buf->size - buf->len + copylen;
		memset(buf->p, spec->pad, width - copylen);
		buf->p += width - copylen;
		buf->len += width - copylen;
	}
	
	if (copylen > buf->size - buf->len)
		copylen = buf->size - buf->len;

	memcpy(buf->p, s, copylen);
	buf->p += copylen;
	buf->len += copylen;
}

static void character(struct buffer *buf, char c)
{
	if (buf->len >= buf->size)
		return;
	*(buf->p++) = c;
	buf->len++;
}

static void integer(struct buffer *buf, uint n, uint sign, uint base,
                    struct spec *spec)
{
	static char *digits = "0123456789abcdef";
	int negative = 0;
	char *s, buf2[64];

	s = buf2 + 64;
	*--s = '\0';

	if (sign && (int)n < 0) {
		negative = 1;
		n = -n;
	}

	if (n == 0) /* special case? */
		*--s = digits[0];

	while (n > 0) {
		*--s = digits[n % base];
		n /= base;

		if (n == 0 && negative)
			*--s = '-';
	}

	/* sneakily hand off spec to string() .. */
	string(buf, s, buf2 + 63 - s, spec);
}

int vsnf(int type, char *s, uint size, const char *fmt, va_list va)
{
	char *p, specbuf[16];
	char c_arg, *s_arg, *q;
	u_user *user;
	u_chan *chan;
	u_server *server;
	u_conn *conn;
	u_sourceinfo *si;

	struct buffer buf;
	struct spec spec;
	int base, debug = 0;
	uint n;

	if (type == FMT_DEBUG) {
		debug = 1;
		type = FMT_LOG;
	}

#ifdef VSNF_LOG
	if (type != FMT_LOG)
		u_log(LG_FINE, "vsnf(%s, %s)",
		      type == FMT_USER ? "USER" : "SERVER", fmt);
#endif

	buf.p = buf.base = s;
	buf.size = size - 1; /* null byte */
	buf.len = 0;

	/* silly little optimization */
	if (streq(fmt, "%s")) {
		s_arg = va_arg(va, char*);
		u_strlcpy(s, s_arg, size);
		return strlen(s);
	}

	/* a goto is used to reduce indentation */
top:
	if (!*fmt)
		goto bottom;

	if (*fmt != '%') {
		character(&buf, *fmt++);
		goto top;
	}

	fmt++;

	base = 0;
	spec.width = 0;
	spec.pad = ' ';

	p = specbuf;
	while (isdigit(*fmt))
		*p++ = *fmt++;
	*p = '\0';

	p = specbuf;
	if (*p) {
		if (*p == '0') {
			spec.pad = '0';
			p++;
		}
		spec.width = atoi(p);
	}

	switch (*fmt) {
	/* useful IRC formats */
	case 'U': /* user */
		user = va_arg(va, u_user*);
		if (type == FMT_SERVER) {
			q = user ? user->uid : "*"; /* XXX: ?????? */
			string(&buf, q, 9, NULL);
		} else {
			q = (user && user->nick[0]) ? user->nick : "*";
			string(&buf, q, -1, &spec);
			if (debug) {
				integer(&buf, (int)user, 0, 16, NULL);
				character(&buf, ']');
			}
		}
		break;

	case 'H': /* hostmask */
		user = va_arg(va, u_user*);
		if (type == FMT_SERVER) {
			string(&buf, user->uid, 9, NULL);
		} else {
			string(&buf, user->nick, -1, NULL);
			character(&buf, '!');
			string(&buf, user->ident, -1, NULL);
			character(&buf, '@');
			string(&buf, user->host, -1, NULL);
			if (debug) {
				character(&buf, '[');
				integer(&buf, (int)user, 0, 16, NULL);
				character(&buf, ']');
			}
		}
		break;

	case 'C': /* channel */
		chan = va_arg(va, u_chan*);
		string(&buf, chan?chan->name:"*", -1, &spec);
		if (debug) {
			character(&buf, '[');
			integer(&buf, (int)chan, 0, 16, NULL);
			character(&buf, ']');
		}
		break;

	case 'S': /* server */
		server = va_arg(va, u_server*);
		if (type == FMT_SERVER) {
			string(&buf, server->sid, 3, NULL);
		} else {
			string(&buf, server->name, -1, &spec);
			if (debug) {
				character(&buf, '[');
				integer(&buf, (int)server, 0, 16, NULL);
				character(&buf, ']');
			}
		}
		break;

	case 'G': /* generic connection */
		conn = va_arg(va, u_conn*);

		switch ((conn && conn->priv) ? conn->ctx : -1) {
		case CTX_USER:
			user = conn->priv;
			s_arg = (type == FMT_SERVER ? user->uid : user->nick);
			break;

		case CTX_SERVER:
			server = conn->priv;
			s_arg = (type == FMT_SERVER ? server->sid : server->name);
			break;

		default:
			s_arg = "*";
		}

		string(&buf, (s_arg && s_arg[0]) ? s_arg : "*", -1, &spec);
		break;

	case 'I': /* sourceinfo */
		si = va_arg(va, u_sourceinfo*);
		if (type == FMT_SERVER) {
			string(&buf, (char*)si->id, si->u ? 9 : 3, NULL);
		} else {
			if (si->u) {
				string(&buf, si->u->nick, -1, NULL);
				character(&buf, '!');
				string(&buf, si->u->ident, -1, NULL);
				character(&buf, '@');
				string(&buf, si->u->host, -1, NULL);
			} else if (si->s) {
				string(&buf, si->s->name, -1, &spec);
			} else {
				string(&buf, "?", 1, NULL);
			}
		}
		break;

	/* standard printf-family formats */
	case 's':
		s_arg = va_arg(va, char*);
		string(&buf, s_arg, -1, &spec);
		break;

	case 'd':
	case 'u':
		base = 10;
	case 'o':
	case 'x':
	case 'p':
		n = va_arg(va, uint);
		if (base == 0) /* eww, further hax */
			base = (*fmt == 'o') ? 8 : 16;

		if (*fmt == 'p') {
			/* this is non-conforming :( */
			spec.width = 8;
			spec.pad = '0';
			string(&buf, "0x", 2, NULL);
		}

		integer(&buf, n, *fmt == 'd', base, &spec);
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

int snf(int type, char *s, uint size, char *fmt, ...)
{
	va_list va;
	int ret;
	va_start(va, fmt);
	ret = vsnf(type, s, size, fmt, va);
	va_end(va);
	return ret;
}

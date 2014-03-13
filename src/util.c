/* Tethys, util.c -- various utilities
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void u_cidr_to_str(u_cidr *cidr, char *s)
{
	struct in_addr in;

	in.s_addr = htonl(cidr->addr);
	u_ntop(&in, s);
	s += strlen(s);
	sprintf(s, "/%d", cidr->netsize);
}

void u_str_to_cidr(char *s, u_cidr *cidr)
{
	struct in_addr in;
	char *p;

	p = strchr(s, '/');
	if (p == NULL) {
		cidr->netsize = 32;
	} else {
		*p++ = '\0';
		cidr->netsize = atoi(p);
		if (cidr->netsize < 0)
			cidr->netsize = 0;
		else if (cidr->netsize > 32)
			cidr->netsize = 32;
	}

	u_aton(s, &in);
	cidr->addr = ntohl(in.s_addr);
}

int u_cidr_match(u_cidr *cidr, char *s)
{
	struct in_addr in;
	ulong mask, addr;

	if (cidr->netsize == 0)
		return 1; /* everything is in /0 */
	/* 8 becomes 0x00ffffff, 21 becomes 0x000007ff, etc */
	mask = (1 << (32 - cidr->netsize)) - 1;

	u_aton(s, &in);
	addr = ntohl(in.s_addr);

	return (addr | mask) == (cidr->addr | mask);
}

void u_bitmask_reset(u_bitmask_set *bm)
{
	*bm = 0;
}

void u_bitmask_used(u_bitmask_set *bm, unsigned long v)
{
	*bm |= v;
}

unsigned long u_bitmask_alloc(u_bitmask_set *bm)
{
	unsigned long v = *bm;

	                   /* find first unset bit */
	                   /* eg,      v = 00011100 11101111 */
	v = v ^ (v + 1);   /*      v + 1 = 00011100 11110000 */
	                   /*    v = xor = 00000000 00011111 */
	v = v ^ (v >> 1);  /*     v >> 1 = 00000000 00001111 */
	                   /*    v = xor = 00000000 00010000 */
	*bm |= v;

	return v;
}

void u_bitmask_free(u_bitmask_set *bm, unsigned long v)
{
	*bm &= ~v;
}

unsigned long parse_size(char *s)
{
	unsigned long n;
	char *mul;

	n = strtoul(s, &mul, 0);

	switch (tolower(*mul)) {
	/* mind the fallthrough */
	case 't': n <<= 10;
	case 'g': n <<= 10;
	case 'm': n <<= 10;
	case 'k': n <<= 10;
	default:
		break;
	}

	return n;
}

#define CT_NICK   001
#define CT_IDENT  002
#define CT_SID    004

static uint ctype_map[256];
static char null_casemap[256];
static char rfc1459_casemap[256];
static char ascii_casemap[256];

int matchmap(char *mask, char *string, char *casemap)
{
	char *m = mask, *s = string;
	char *m_bt = m, *s_bt = s;
 
	for (;;) {
		switch (*m) {
		case '\0':
			if (*s == '\0')
				return 1;
		backtrack:
			if (m_bt == mask)
				return 0;
			m = m_bt;
			s = ++s_bt;
			break;
 
		case '*':
			while (*m == '*')
				m++;
			m_bt = m;
			s_bt = s;
			break;
 
		default:
			if (!*s)
				return 0;
			if (casemap[(uchar)*m] != casemap[(uchar)*s]
			    && *m != '?')
				goto backtrack;
			m++;
			s++;
		}
	}
}

int match(char *mask, char *string)
{
	return matchmap(mask, string, null_casemap);
}

int matchirc(char *mask, char *string)
{
	return matchmap(mask, string, rfc1459_casemap);
}

int matchcase(char *mask, char *string)
{
	return matchmap(mask, string, ascii_casemap);
}

int matchhash(char *hash, char *string)
{
	char buf[CRYPTLEN];
	u_crypto_hash(buf, string, hash);
	return streq(buf, hash);
}

int mapcmp(char *s1, char *s2, char *map)
{
	int diff;

	while (*s1 && *s2) {
		diff = map[(ulong)*s1] - map[(ulong)*s2];
		if (diff != 0)
			return diff;
		s1++;
		s2++;
	}

	return map[(ulong)*s1] - map[(ulong)*s2];
}

int casecmp(char *s1, char *s2)
{
	return mapcmp(s1, s2, ascii_casemap);
}

int irccmp(char *s1, char *s2)
{
	return mapcmp(s1, s2, rfc1459_casemap);
}

void u_ntop(struct in_addr *in, char *s)
{
	u_strlcpy(s, inet_ntoa(*in), INET_ADDRSTRLEN);
}

void u_aton(char *s, struct in_addr *in)
{
	in->s_addr = inet_addr(s);
}

void u_pton(const char *src, struct sockaddr_storage *ss)
{
	void *v4, *v6;

	memset(ss, 0, sizeof(*ss));

	v4 = &((struct sockaddr_in*) ss)->sin_addr;
	v6 = &((struct sockaddr_in6*)ss)->sin6_addr;

	if (!(inet_pton(AF_INET, src, v4) < 0)) {
		ss->ss_family = AF_INET;
		return;
	}

	if (!(inet_pton(AF_INET6, src, v6) < 0)) {
		ss->ss_family = AF_INET6;
		return;
	}
}

char *cut(char **p, char *delim)
{
	char *s = *p;

	if (s == NULL)
		return NULL;

	while (**p && !strchr(delim, **p))
		(*p)++;

	if (!**p) {
		*p = NULL;
	} else {
		*(*p)++ = '\0';
		while (strchr(delim, **p))
			(*p)++;
	}

	return s;
}

void null_canonize(char *s)
{
}

void rfc1459_canonize(char *s)
{
	for (; *s; s++)
		*s = rfc1459_casemap[(uchar)*s];
}

void ascii_canonize(char *s)
{
	for (; *s; s++)
		*s = ascii_casemap[(uchar)*s];
}

char *id_to_name(char *s)
{
	if (s[3]) { /* uid */
		u_user *u = u_user_by_uid(s);
		return u ? u->nick : "*";
	} else { /* sid */
		u_server *sv = u_server_by_sid(s);
		return sv ? sv->name : "*";
	}
}

char *name_to_id(char *s)
{
	if (strchr(s, '.')) { /* server */
		u_server *sv = u_server_by_name(s);
		return sv ? (sv->sid[0] ? sv->sid : NULL) : NULL;
	} else {
		u_user *u = u_user_by_nick(s);
		return u ? u->uid : NULL;
	}
}

char *ref_to_name(char *s)
{
	if (isdigit(s[0]))
		return id_to_name(s);
	return s;
}

char *ref_to_id(char *s)
{
	if (isdigit(s[0]))
		return s;
	return name_to_id(s);
}

bool exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

char *ref_to_ref(u_link *ctx, char *ref)
{
	return ctx->type == LINK_SERVER ? ref_to_id(ref) : ref_to_name(ref);
}

u_link *ref_link(u_link *ctx, char *ref)
{
	if (ctx && ctx->type == LINK_SERVER && isdigit(ref[0])) {
		if (ref[3]) {
			u_user *u = u_user_by_uid(ref);
			return u ? u->link : NULL;
		} else {
			u_server *sv = u_server_by_sid(ref);
			return sv ? sv->link : NULL;
		}
	} else {
		if (!strchr(ref, '.')) {
			u_user *u = u_user_by_nick(ref);
			return u ? u->link : NULL;
		} else {
			u_server *sv = u_server_by_name(ref);
			return sv ? sv->link : NULL;
		}
	}
}

char *link_name(u_link *link)
{
	char *name = NULL;

	switch (link->type) {
	case LINK_USER:
		name = ((u_user*)link->priv)->nick;
		break;
	case LINK_SERVER:
		name = SERVER(link->priv)->name;
		break;
	default:
		break;
	}

	return (name && name[0]) ? name : "*";
}

char *link_id(u_link *link)
{
	char *id = NULL;

	switch (link->type) {
	case LINK_USER:
		id = ((u_user*)link->priv)->uid;
		break;
	case LINK_SERVER:
		id = SERVER(link->priv)->sid;
	default:
		break;
	}

	return (id && id[0]) ? id : NULL;
}

int is_valid_nick(char *s)
{
	if (isdigit(*s) || *s == '-')
		return 0;
	for (; *s; s++) {
		if (!(ctype_map[(uchar)*s] & CT_NICK))
			return 0;
	}
	return 1;
}

int is_valid_ident(char *s)
{
	for (; *s; s++) {
		if (!(ctype_map[(uchar)*s] & CT_IDENT))
			return 0;
	}
	return 1;
}

int is_valid_sid(char *s)
{
	if (strlen(s) != 3)
		return 0;
	if (!isdigit(s[0]))
		return 0;
	for (; *s; s++) {
		if (!(ctype_map[(uchar)*s] & CT_SID))
			return 0;
	}
	return 1;
}

int is_valid_chan(char *s)
{
	if (!s || !strchr(CHANTYPES, *s))
		return 0;
	if (strchr(s, ' '))
		return 0;
	return 1;
}

int set_cloexec(int fd)
{
#ifdef FD_CLOEXEC
	int flags;

	if ((flags = fcntl(fd, F_GETFD)) < 0)
		return -1;

	if (fcntl(fd, F_SETFD, flags|FD_CLOEXEC) < 0)
		return -1;
#endif

	return 0;
}

/* Base64 Encoder
 * --------------
 */
static const char _table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t
base64_encode(
	const void *in_buf, size_t in_buf_len,
	char *out_buf, char *out_cur)
{
	size_t i, j, jsub = 0;
	uint32_t triple = 0;
	uint8_t nb = 0;
	const char *ib = in_buf;
	uint8_t joinbuf[3] = {};

	if (out_cur >= out_buf + 4 && out_cur[-1] == '=') {
		/* This is a little odd so I'll explain. Each sendq is split into chunks.
		 * We have to call base64_encode separately for each chunk. In the worst
		 * case scenario, each chunk results in two padding characters (==). This
		 * throws off the worst-case size estimate of base64_inflate_size, which
		 * assumes a single call and only two padding characters. We want to be
		 * able to estimate the size of the base64 encoded data so we can allocate
		 * the entire memory needed up front. We need it all in contiguous memory
		 * so that it can form a mowgli_string_t and thus form part of the JSON
		 * tree.
		 *
		 * The obvious choice is to make the base64 encoder resumable. For ordinary
		 * operation, pass out_buf == out_cur. When out_cur > out_buf, the few
		 * bytes before out_cur are examined to see if there's any padding. If so,
		 * the base64 "quartet" is decoded, the pointer out_cur adjusted, and we
		 * naturally join the two streams together. Essentially the state of the
		 * base64 encoder when previously called is loaded from the buffer itself.
		 *
		 * Since base64 outputs in "quartets" of four characters, if out_cur >
		 * out_buf then out_cur must >= out_buf + 4. We are assuming we have a
		 * valid base64 stream that we generated ourselves. If this is not the
		 * case, just generate ordinarily; the concatenation of two base64 streams
		 * with padding is itself valid base64; in this case, caller beware.
		 */
		out_cur -= 4, jsub = 4;
		nb = (uint8_t)base64_decode(out_cur, 4, joinbuf);
		/* We know we have padding. Padding is only used to map 1 or 2 bytes to a
		 * base64 quartet, so nb == 1 or nb == 2. So this is a correct evaluation
		 * of the main for loop further below for this particular case, only on
		 * joinbuf, not ib.
		 */
		for (i=0; i<nb; ++i)
			triple = (triple << 8) | joinbuf[i];
		/* (I was not honestly expecting to convert a non-resumable base64 encoder
		 * into a resumable base64 encoder in five lines of code. Huh.) */
	}

	for (i=0,j=0; i<in_buf_len; ++i) {
		triple = (triple << 8) | ib[i];
		if (++nb == 3) {
			out_cur[j++] = _table[(triple >> 3*6) & 0x3F];
			out_cur[j++] = _table[(triple >> 2*6) & 0x3F];
			out_cur[j++] = _table[(triple >> 1*6) & 0x3F];
			out_cur[j++] = _table[(triple >> 0*6) & 0x3F];
			nb = 0, triple = 0;
		}
	}

	switch (nb) {
		case 2:
			triple <<= 8;
			out_cur[j++] = _table[(triple >> 3*6) & 0x3F];
			out_cur[j++] = _table[(triple >> 2*6) & 0x3F];
			out_cur[j++] = _table[(triple >> 1*6) & 0x3F];
			out_cur[j++] = '=';
			break;
		case 1:
			triple <<= 2*8;
			out_cur[j++] = _table[(triple >> 3*6) & 0x3F];
			out_cur[j++] = _table[(triple >> 2*6) & 0x3F];
			out_cur[j++] = '=';
			out_cur[j++] = '=';
			break;
	}

	return j - jsub;
}

/* Base64 Decoder
 * --------------
 */
static char _dectbl[256] = {};

static void _init_dectbl(void)
{
	static bool _done = false;
	int i;

	if (_done)
		return;

	for (i=0; i<64; ++i)
		_dectbl[(uint8_t)_table[i]] = i;

	_done = true;
}

size_t
base64_decode(
	const char *in_buf, size_t in_buf_len,
	void *out_buf)
{
	size_t i, written = 0;
	uint8_t *ob = out_buf;
	uint8_t nc = 0, np = 0;
	uint32_t triple;
	char c[4];

	_init_dectbl();

	for (i=0;i<in_buf_len;++i) {
		if (in_buf[i] == '=') {
			c[nc++] = 0;
			np = (np+1) % 3;
		} else {
			c[nc++] = _dectbl[(uint8_t)in_buf[i]];
		}

		if (nc == 4) {
			triple = (c[0] << 3*6) | (c[1] << 2*6) | (c[2] << 1*6) | (c[3] << 0*6);
			ob[0] = (uint8_t)(triple >> 2*8);
			ob[1] = (uint8_t)(triple >> 1*8);
			ob[2] = (uint8_t)(triple >> 0*8);
			written += 3 - np;
			ob      += 3 - np;
			nc = np = 0;
		}
	}

	return written;
}

/* JSON
 * ----
 */
void json_osetb64(mowgli_json_t *obj, const char *k, const void *buf, size_t buf_len)
{
	char *b64buf;
	size_t b64len;
	size_t written;

	b64len = base64_inflate_size(buf_len);
	b64buf = malloc(b64len);

	written = base64_encode(buf, buf_len, b64buf, b64buf);
	b64buf[written] = '\0';

	json_osets(obj, k, b64buf);

	free(b64buf);
}

ssize_t json_ogetb64(mowgli_json_t *obj, const char *k, void *buf, size_t buf_len)
{
	mowgli_string_t *s;
	size_t len;

	s = json_ogets(obj, k);
	if (!s)
		return -1;

	len = base64_deflate_size(s->pos);
	if (len > buf_len)
		return -1;

	return base64_decode(s->str, s->pos, buf);
}

/* Initialization
 * --------------
 */
int init_util(void)
{
	int i;

	for (i=0; i<256; i++) {
		ctype_map[i] = 0;
		if (isalnum(i) || strchr("[]{}|\\^-_`", i))
			ctype_map[i] |= CT_NICK;
		if (isalnum(i) || strchr("[]{}-_", i))
			ctype_map[i] |= CT_IDENT;
		if (isalnum(i))
			ctype_map[i] |= CT_SID;

		null_casemap[i] = i;
		rfc1459_casemap[i] = islower(i) ? toupper(i) : i;
		ascii_casemap[i] = islower(i) ? toupper(i) : i;
	}

	rfc1459_casemap['['] = '{';
	rfc1459_casemap[']'] = '}';
	rfc1459_casemap['\\'] = '|';
	rfc1459_casemap['~'] = '^';

	return 0;
}

/* vim: set noet: */

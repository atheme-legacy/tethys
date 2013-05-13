/* ircd-micro, util.c -- various utilities
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void u_cidr_to_str(cidr, s) u_cidr *cidr; char *s;
{
	struct in_addr in;

	in.s_addr = htonl(cidr->addr);
	u_ntop(&in, s);
	s += strlen(s);
	sprintf(s, "/%d", cidr->netsize);
}

void u_str_to_cidr(s, cidr) char *s; u_cidr *cidr;
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

int u_cidr_match(cidr, s) u_cidr *cidr; char *s;
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

#define CT_NICK   001
#define CT_IDENT  002
#define CT_SID    004

static uint ctype_map[256];
static char null_casemap[256];
static char rfc1459_casemap[256];
static char ascii_casemap[256];

int matchmap(mask, string, casemap) char *mask, *string, *casemap;
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

int match(mask, string) char *mask, *string;
{
	return matchmap(mask, string, null_casemap);
}

int matchirc(mask, string) char *mask, *string;
{
	return matchmap(mask, string, rfc1459_casemap);
}

int matchcase(mask, string) char *mask, *string;
{
	return matchmap(mask, string, ascii_casemap);
}

int matchhash(hash, string) char *hash, *string;
{
	char buf[CRYPTLEN];
	u_crypto_hash(buf, string, hash);
	return streq(buf, hash);
}

int mapcmp(s1, s2, map) char *s1, *s2, *map;
{
	int diff;

	while (*s1 && *s2) {
		diff = map[(unsigned)*s1] - map[(unsigned)*s2];
		if (diff != 0)
			return diff;
		s1++;
		s2++;
	}

	return map[(unsigned)*s1] - map[(unsigned)*s2];
}

int casecmp(s1, s2) char *s1, *s2;
{
	return mapcmp(s1, s2, ascii_casemap);
}

int irccmp(s1, s2) char *s1, *s2;
{
	return mapcmp(s1, s2, rfc1459_casemap);
}

void u_memmove_lower(dest, src, n) char *dest, *src;
{
	while (n --> 0) {
		*dest++ = *src++;
	}
}

void u_memmove_upper(dest, src, n) char *dest, *src;
{
	src += n;
	dest += n;
	while (n --> 0) {
		*--dest = *--src;
	}
}

void u_memmove(dest, src, n) char *dest, *src;
{
	if (dest < src)
		u_memmove_lower(dest, src, n);
	if (dest > src)
		u_memmove_upper(dest, src, n);
}

void u_strlcpy(dest, src, n) char *dest, *src;
{
	int len;
	n--;

	len = strlen(src);
	if (n > len)
		n = len;

	memcpy(dest, src, n);
	dest[n] = '\0';
}

void u_strlcat(dest, src, n) char *dest, *src;
{
	int len = strlen(dest);
	u_strlcpy(dest+len, src, n-len);
}

char *u_strdup(s) char *s;
{
	int len = strlen(s) + 1;
	char *p = malloc(len);
	memcpy(p, s, len);
	return p;
}

void u_ntop(in, s) struct in_addr *in; char *s;
{
	u_strlcpy(s, inet_ntoa(*in), INET_ADDRSTRLEN);
}

void u_aton(s, in) char *s; struct in_addr *in;
{
	in->s_addr = inet_addr(s);
}

char *cut(p, delim) char **p, *delim;
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

/* example usage:
   char *s, buf[51];
   int i;
   s = buf;
   for (i=0; i<parc; ) {
           if (!wrap(buf, &s, 50, parv[i])) {
                   puts(buf);
                   continue;
           }
           i++;
   }
   if (s != buf)
           puts(buf);
 */
int wrap(base, p, w, str) char *base, **p, *str; uint w;
{
	uint len = strlen(str) + (base != *p);

	if (*p + len > base + w) {
		*p = base;
		return 0;
	}

	if (base != *p) {
		*(*p)++ = ' ';
		len--;
	}

	memcpy(*p, str, len);
	*p += len;
	**p = '\0';

	return 1;
}

void null_canonize(s) char *s;
{
}

void rfc1459_canonize(s) char *s;
{
	for (; *s; s++)
		*s = rfc1459_casemap[(uchar)*s];
}

void ascii_canonize(s) char *s;
{
	for (; *s; s++)
		*s = ascii_casemap[(uchar)*s];
}

char *id_to_name(s) char *s;
{
	if (s[4]) { /* uid */
		u_user *u = u_user_by_uid(s);
		return u ? u->nick : "*";
	} else { /* sid */
		u_server *sv = u_server_by_sid(s);
		return sv ? sv->name : "*";
	}
}

char *name_to_id(s) char *s;
{
	if (strchr(s, '.')) { /* server */
		u_server *sv = u_server_by_name(s);
		return sv ? sv->sid : NULL;
	} else {
		u_user *u = u_user_by_nick(s);
		return u ? u->uid : NULL;
	}
}

char *ref_to_name(s) char *s;
{
	if (isdigit(s[0]))
		return id_to_name(s);
	return s;
}

char *ref_to_id(s) char *s;
{
	if (isdigit(s[0]))
		return s;
	return name_to_id(s);
}

char *conn_name(conn) u_conn *conn;
{
	char *name = NULL;

	switch (conn->ctx) {
	case CTX_USER:
	case CTX_UREG:
		name = USER(conn->priv)->nick;
		break;
	case CTX_SREG:
	case CTX_SERVER:
	case CTX_SBURST:
		name = SERVER(conn->priv)->name;
		break;
	}

	return (name && name[0]) ? name : "*";
}

char *conn_id(conn) u_conn *conn;
{
	char *id = NULL;

	switch (conn->ctx) {
	case CTX_USER:
	case CTX_UREG:
		id = USER(conn->priv)->uid;
		break;
	case CTX_SREG:
	case CTX_SERVER:
	case CTX_SBURST:
		id = SERVER(conn->priv)->sid;
		break;
	}

	return (id && id[0]) ? id : NULL;
}

int is_valid_nick(s) char *s;
{
	if (isdigit(*s) || *s == '-')
		return 0;
	for (; *s; s++) {
		if (!(ctype_map[(uchar)*s] & CT_NICK))
			return 0;
	}
	return 1;
}

int is_valid_ident(s) char *s;
{
	for (; *s; s++) {
		if (!(ctype_map[(uchar)*s] & CT_IDENT))
			return 0;
	}
	return 1;
}

int is_valid_sid(s) char *s;
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

int init_util()
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

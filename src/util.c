/* ircd-micro, util.c -- various utilities
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static char null_casemap[256];
static char rfc1459_casemap[256];
static char ascii_casemap[256];
static int valid_nick_map[256];
static int valid_ident_map[256];

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
			if (casemap[(unsigned char)*m] != casemap[(unsigned char)*s]
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
int wrap(base, p, w, str) char *base, **p, *str; unsigned w;
{
	unsigned len = strlen(str) + (base != *p);

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
		*s = rfc1459_casemap[(unsigned char)*s];
}

void ascii_canonize(s) char *s;
{
	for (; *s; s++)
		*s = ascii_casemap[(unsigned char)*s];
}

int is_valid_nick(s) char *s;
{
	if (isdigit(*s))
		return 0;
	for (; *s; s++) {
		if (!valid_nick_map[(unsigned char)*s])
			return 0;
	}
	return 1;
}

int is_valid_ident(s) char *s;
{
	for (; *s; s++) {
		if (!valid_ident_map[(unsigned char)*s])
			return 0;
	}
	return 1;
}

int init_util()
{
	int i;

	for (i=0; i<256; i++) {
		null_casemap[i] = i;
		rfc1459_casemap[i] = islower(i) ? toupper(i) : i;
		ascii_casemap[i] = islower(i) ? toupper(i) : i;
	}
	rfc1459_casemap['['] = '{';
	rfc1459_casemap[']'] = '}';
	rfc1459_casemap['\\'] = '|';
	rfc1459_casemap['~'] = '^';

	for (i=0; i<256; i++)
		valid_nick_map[i] = isalnum(i) || strchr("[]{}|\\^-_", i);

	/* TODO: decide */
	for (i=0; i<256; i++)
		valid_ident_map[i] = isalnum(i) || strchr("[]{}-_", i);

	return 0;
}

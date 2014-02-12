/* Tethys, strop.c -- advanced string operations
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void u_strop_split_start(u_strop_state *st, char *s, char *delim)
{
	st->s = s;
	st->p = delim;
}

char *u_strop_split_next(u_strop_state *st)
{
	char *t, *u;

	t = st->s;
	while (*t && strchr(st->p, *t))
		t++;

	u = t;
	while (*u && !strchr(st->p, *u))
		u++;
	if (*u) *u++ = '\0';

	st->s = u;
	return *t ? t : NULL;
}


void u_strop_wrap_start(u_strop_wrap *w, size_t width)
{
	w->again = NULL;
	w->sz = 0;
	w->width = width;
	if (w->width > U_STROP_WRAP_MAX)
		w->width = U_STROP_WRAP_MAX;
}

char *u_strop_wrap_word(u_strop_wrap *w, char *word)
{
	size_t n;

	if (word == NULL) {
		char *s = w->sz ? w->buf : NULL;
		w->sz = 0;
		return s;
	}

	if (w->again) {
		word = w->again;
		w->again = NULL;
	}

	n = strlen(word);

	if (w->sz + n + 1 > w->width) {
		if (w->sz == 0) {
			snprintf(w->buf, w->width + 1, "%s", word);
			w->again = word + w->width;
		}

		w->sz = 0;
		return w->buf;
	}

	w->sz += snprintf(w->buf + w->sz, U_STROP_WRAP_MAX + 1 - w->sz,
	                  "%s%s", w->sz ? " " : "", word);

	return NULL;
}

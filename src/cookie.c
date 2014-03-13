/* Tethys, cookie.c -- unique, comparable cookies
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void u_cookie_reset(u_cookie *ck)
{
	ck->high = ck->low = 0;
}

void u_cookie_inc(u_cookie *ck)
{
	if (ck->high == NOW.tv_sec) {
		ck->low ++;
		return;
	}

	ck->high = NOW.tv_sec;
	ck->low = 0;
}

void u_cookie_cpy(u_cookie *a, u_cookie *b)
{
	memcpy(a, b, sizeof(*a));
}

static int norm(int x)
{
	if (x < 0) return -1;
	if (x > 0) return 1;
	return 0;
}

int u_cookie_cmp(u_cookie *a, u_cookie *b)
{
	if (a->high == b->high)
		return norm(a->low - b->low);
	return norm(a->high - b->high);
}

mowgli_json_t *u_cookie_to_json(u_cookie *a)
{
	mowgli_json_t *jc;

	jc = mowgli_json_create_object();
	json_osetul(jc, "high", a->high);
	json_osetul(jc, "low",  a->low);

	return jc;
}

int u_cookie_from_json(mowgli_json_t *jc, u_cookie *c)
{
	int err;

	if ((err = json_ogetul(jc, "high", &c->high)) < 0)
		return err;
	if ((err = json_ogetul(jc, "low",  &c->low)) < 0)
		return err;

	return 0;
}

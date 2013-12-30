/* ircd-micro, cookie.h -- unique, comparable cookies
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_COOKIE_H__
#define __INC_COOKIE_H__

typedef struct u_cookie u_cookie;

struct u_cookie {
	ulong high, low;
};

extern void u_cookie_reset(u_cookie*);
extern void u_cookie_inc(u_cookie*);
extern void u_cookie_cpy(u_cookie *A, u_cookie *B);
extern int u_cookie_cmp(u_cookie*, u_cookie*);

#endif

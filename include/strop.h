/* Tethys, strop.h -- advanced string operations
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_STROP_H__
#define __INC_STROP_H__

typedef struct u_strop_state u_strop_state;

struct u_strop_state {
	char *s, *p;
	char buf[1024];
};


/* splitting */

#define U_STROP_SPLIT(STATE, STRING, DELIM, LOC) \
	for (u_strop_split_start((STATE), (STRING), (DELIM)); \
	     *(LOC) = u_strop_split_next((STATE)); )

extern void u_strop_split_start(u_strop_state*, char*, char*);
extern char *u_strop_split_next(u_strop_state*);


/* wrapping */

#define U_STROP_WRAP_MAX 1024

typedef struct u_strop_wrap u_strop_wrap;

struct u_strop_wrap {
	char *again;
	char buf[U_STROP_WRAP_MAX+1];
	size_t sz, width;
};

extern void u_strop_wrap_start(u_strop_wrap*, size_t width);
extern char *u_strop_wrap_word(u_strop_wrap*, char*);

#endif

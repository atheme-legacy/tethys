/* ircd-micro, linebuf.h -- line buffer
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_LINEBUF_H__
#define __INC_LINEBUF_H__

#define U_LINEBUF_SIZE 1024

typedef struct u_linebuf u_linebuf;

struct u_linebuf {
	char buf[U_LINEBUF_SIZE];
	unsigned pos;
};

extern void u_linebuf_init(); /* u_linebuf* */
extern int u_linebuf_data(); /* u_linebuf*, char*, int */
extern int u_linebuf_line(); /* u_linebuf*, char*, int */

#endif

/* Tethys, mode.h -- mode processing
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_MODE_H__
#define __INC_MODE_H__

#define MODE_BUFSIZE 512

typedef struct u_mode_info u_mode_info;
typedef struct u_mode_changes u_mode_changes;
typedef struct u_modes u_modes;

struct u_mode_info {
	char ch;
	/* return bool, if used arg or not */
	int (*cb)(u_modes*, int on, char *arg);
	ulong data;
};

struct u_mode_changes {
	int setting;
	char *b, buf[MODE_BUFSIZE];
	char *d, data[MODE_BUFSIZE];
};

struct u_modes {
	/* opaque, untouched by mode */
	void *setter;
	void *target;
	void *perms;
	ulong flags;

	/* in callback, currently-processed info */
	u_mode_info *info;

	/* string buffers, edited by u_mode_put functions */
	u_mode_changes u;
	u_mode_changes s;
	char *l, list[32];
};

extern void u_mode_put(u_modes*, int on, char ch, char *fmt, void *p);
extern void u_mode_put_u(u_modes*, int on, char ch, char *fmt, void *p);
extern void u_mode_put_s(u_modes*, int on, char ch, char *fmt, void *p);
extern void u_mode_put_l(u_modes*, char ch);

extern void u_mode_process(u_modes *m, u_mode_info *info, int parc, char **parv);

#endif

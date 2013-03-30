/* ircd-micro, chan.h -- channels
   Copyright (c) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_CHAN_H__
#define __INC_CHAN_H__

#define MAXCHANNAME 50
#define MAXTOPICLEN 250

/* channel modes */
#define CMODE_PRIVATE      0x00000001  /* +p */
#define CMODE_SECRET       0x00000002  /* +s */
#define CMODE_INVITEONLY   0x00000004  /* +i */
#define CMODE_TOPIC        0x00000008  /* +t */
#define CMODE_NOEXTERNAL   0x00000010  /* +n */
#define CMODE_MODERATED    0x00000020  /* +m */
#define CMODE_OPMOD        0x00000040  /* +z */
#define CMODE_NOCOLOR      0x00000080  /* +c */
#define CMODE_FREEINVITE   0x00000100  /* +g */

/* prefixes */
#define CU_PFX_OP          0x00000001
#define CU_PFX_VOICE       0x00000002

#define CU_MUTED           0x00010000

struct u_cmode_info {
	char ch;
	/* int cb(u_cmode_info*, u_chan*, on, char *(*getarg)())
	   on is 1 if +, 0 if -
	   getarg takes no arguments; returns NULL if no more args
	   cb returns -1 if it was unable to process the mode */
	int (*cb)();
	unsigned data;
};

struct u_chan {
	char name[MAXCHANNAME+1];
	char topic[MAXTOPICLEN+1];
	char topic_setter[MAXNICKLEN+1];
	u_ts_t topic_time;
	unsigned mode;
	struct u_cookie ck_flags;
	struct u_map *members;
	struct u_list ban, quiet, banex, invex;
	char *forward, *key;
};

struct u_chanuser {
	unsigned flags;
	struct u_cookie ck_flags;
	struct u_chan *c;
	struct u_user *u;
};

extern struct u_cmode_info *cmodes;
extern unsigned cmode_default;

extern struct u_chan *u_chan_get(); /* char* */
extern struct u_chan *u_chan_get_or_create(); /* char* */
extern void u_chan_drop(); /* struct u_chan* */

extern char *u_chan_modes(); /* struct u_chan* */

extern void u_chan_send_topic(); /* u_chan*, u_user* */

extern struct u_chanuser *u_chan_user_add(); /* u_chan*, u_user* */
extern void u_chan_user_del(); /* u_chanuser* */
extern struct u_chanuser *u_chan_user_find(); /* u_chan*, u_user* */

extern int init_chan();

#endif

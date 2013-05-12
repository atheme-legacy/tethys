/* ircd-micro, chan.h -- channels
   Copyright (c) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_CHAN_H__
#define __INC_CHAN_H__

#define MAXCHANNAME 50
#define MAXTOPICLEN 250
#define MAXBANLIST  50

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

typedef struct u_cmode_info u_cmode_info;
typedef struct u_chan u_chan;
typedef struct u_chanuser u_chanuser;
typedef struct u_chanban u_chanban;

struct u_cmode_info {
	char ch;
	/* int cb(u_cmode_info*, u_chan*, u_user*, on, char *(*getarg)())
	   on is 1 if +, 0 if -
	   getarg takes no arguments; returns NULL if no more args */
	void (*cb)();
	uint data;
};

struct u_chan {
	u_ts_t ts;
	char name[MAXCHANNAME+1];
	char topic[MAXTOPICLEN+1];
	char topic_setter[MAXNICKLEN+1];
	u_ts_t topic_time;
	uint mode;
	u_cookie ck_flags;
	u_map *members;
	u_list ban, quiet, banex, invex;
	char *forward, *key;
	int limit;
};

struct u_chanuser {
	uint flags;
	u_cookie ck_flags;
	u_chan *c;
	u_user *u;
};

struct u_chanban {
	char mask[256];
	char setter[256];
	u_ts_t time;
};

extern u_trie *all_chans;

extern u_cmode_info *cmodes;
extern uint cmode_default;

/* start and end of mode processing */
extern void u_chan_m_start(); /* u_user*, u_chan* */
extern void u_chan_m_end(); /* u_user*, u_chan* */

extern u_chan *u_chan_get(); /* char* */
extern u_chan *u_chan_get_or_create(); /* char* */
extern void u_chan_drop(); /* u_chan* */

extern char *u_chan_modes(); /* u_chan*, u_chanuser* */
extern void u_chan_mode(); /* u_chan*, u_user*, char chr, int on, getarg() */

extern void u_chan_send_topic(); /* u_chan*, u_user* */
extern void u_chan_send_names(); /* u_chan*, u_user* */
extern void u_chan_send_list(); /* u_chan*, u_user*, u_list* */

extern u_chanuser *u_chan_user_add(); /* u_chan*, u_user* */
extern void u_chan_user_del(); /* u_chanuser* */
extern u_chanuser *u_chan_user_find(); /* u_chan*, u_user* */

extern int u_entry_blocked(); /* u_chan*, u_user*, char *key */
extern int u_is_muted(); /* u_chanuser* */

extern int init_chan();

#endif

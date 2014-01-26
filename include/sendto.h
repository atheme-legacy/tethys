/* Tethys, sendto.h -- unified message sending
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_SENDTO_H__
#define __INC_SENDTO_H__

#define ST_ALL                 0
#define ST_SERVERS             1
#define ST_USERS               2

extern void u_st_start(void);
extern void u_st_exclude(u_conn *conn);

/* second argument is excluded from message */
extern void u_sendto_chan(u_chan*, u_conn*, uint, char*, ...);

/* sends to all connections a user is visible to */
extern void u_sendto_visible(u_user*, uint, char*, ...);

typedef struct u_sendto_state u_sendto_state;

struct u_sendto_state {
	u_map_each_state chans;
	u_map_each_state members;
	u_chan *c;
	uint type;
};

extern void u_sendto_chan_start(u_sendto_state*, u_chan*, u_conn*, uint);
extern bool u_sendto_chan_next(u_sendto_state*, u_conn**);

#define U_SENDTO_CHAN(STATE, CHAN, EXCLUDE, TYPE, CONN) \
	for (u_sendto_chan_start((STATE), (CHAN), (EXCLUDE), (TYPE)); \
	     u_sendto_chan_next((STATE), &(CONN)); )

extern void u_sendto_visible_start(u_sendto_state*, u_user*, u_conn*, uint);
extern bool u_sendto_visible_next(u_sendto_state*, u_conn**);

#define U_SENDTO_VISIBLE(STATE, USER, EXCLUDE, TYPE, CONN) \
	for (u_sendto_visible_start((STATE), (USER), (EXCLUDE), (TYPE)); \
	     u_sendto_visible_next((STATE), &(CONN)); )

/*
      +-----------------------------------------------+
   00 |XX|+w|                                         |
      +--+--+          SIMPLE ROSTERS              +--+
      |                                            |sv| 1f
      +-----------------------------------------------+
      /                   (UNUSED)                    /
      +-----------------------------------------------+
   c0 |                                               |
      |                   SNOMASKS                    |
      |                                               | ff
      +-----------------------------------------------+
 */

#define R_WALLOPS      0x01  /* local +w users */
#define R_SERVERS      0x1f  /* locally connected servers */

#define R_SNOTICE(c)   (0xc0 | ((c) & 0x3f))

extern void u_roster_add(uchar r, u_conn*);
extern void u_roster_del(uchar r, u_conn*);
extern void u_roster_del_all(u_conn*);
extern void u_roster_f(uchar, u_conn*, char*, ...);

extern void u_wallops(char*, ...);

extern int init_sendto(void);

#endif

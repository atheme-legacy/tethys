/* Tethys, sendto.h -- unified message sending
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_SENDTO_H__
#define __INC_SENDTO_H__

#define ST_ALL                 0
#define ST_SERVERS             1
#define ST_USERS               2
#define ST_STOP       ((uint)-1)

/* manual sendto interface. If using the iterators below, you should
   still use u_sendto to send messages, or risk duplication */

extern void u_sendto_start(void);
extern void u_sendto_skip(u_conn*);

extern void u_sendto(u_conn*, char*, ...);

/* sends message to the various places. these implementations are based
   on the sendto iterators below, but are careful not to repeat format
   evaluation more times than necessary.

   the u_conn* arguments are a connection to skip over, e.g. when
   propagating a server message */

extern void u_sendto_chan(u_chan*, u_conn*, uint, char*, ...);
extern void u_sendto_visible(u_user*, uint, char*, ...);
extern void u_sendto_servers(u_conn*, char*, ...);
extern void u_sendto_list(mowgli_list_t *list, u_conn*, char*, ...);
extern void u_sendto_map(u_map *map, u_conn*, char*, ...); /* to values */

typedef struct u_sendto_state u_sendto_state;

struct u_sendto_state {
	u_map_each_state chans;
	u_map_each_state members;
	u_chan *c;
	uint type;
	mowgli_patricia_iteration_state_t pstate;
};

extern void u_sendto_chan_start(u_sendto_state*, u_chan*, u_conn*, uint);
extern bool u_sendto_chan_next(u_sendto_state*, u_conn**);

#define U_SENDTO_CHAN(STATE, CHAN, EXCLUDE, TYPE, CONN) \
	for (u_sendto_chan_start((STATE), (CHAN), (EXCLUDE), (TYPE)); \
	     u_sendto_chan_next((STATE), (CONN)); )

extern void u_sendto_visible_start(u_sendto_state*, u_user*, u_conn*, uint);
extern bool u_sendto_visible_next(u_sendto_state*, u_conn**);

#define U_SENDTO_VISIBLE(STATE, USER, EXCLUDE, TYPE, CONN) \
	for (u_sendto_visible_start((STATE), (USER), (EXCLUDE), (TYPE)); \
	     u_sendto_visible_next((STATE), (CONN)); )

extern void u_sendto_servers_start(u_sendto_state*, u_conn*);
extern bool u_sendto_servers_next(u_sendto_state*, u_conn**);

#define U_SENDTO_SERVERS(STATE, EXCLUDE, CONN) \
	for (u_sendto_servers_start((STATE), (EXCLUDE)); \
	     u_sendto_servers_next((STATE), (CONN)); )

extern int init_sendto(void);

#endif

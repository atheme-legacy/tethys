/* ircd-micro, sendto.h -- unified message sending
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_SENDTO_H__
#define __INC_SENDTO_H__

#define ST_ALL                 0
#define ST_SERVERS             1
#define ST_USERS               2

typedef struct u_st_opts u_st_opts;

struct u_st_opts {
	uint type;

	uint flags_all;
	uint flags_none;

	u_chan *c;
	uint cu_flags;

	char *serv, *user, *fmt;
	va_list va;
};

extern void u_st_start(void);
extern void u_st_exclude(u_conn *conn);
extern int u_st_match_server(u_st_opts*, u_server*);
extern int u_st_match_user(u_st_opts*, u_user*);
extern int u_st_match_conn(u_st_opts*, u_conn*);

/* second argument is excluded from message */
extern void u_sendto_chan(u_chan*, u_conn*, uint, char*, ...);

/* sends to all connections a user is visible to */
extern void u_sendto_visible(u_user*, uint, char*, ...);

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

extern void u_roster_add(unsigned char r, u_conn*);
extern void u_roster_del(unsigned char r, u_conn*);
extern void u_roster_del_all(u_conn*);
extern void u_roster_f(unsigned char, u_conn*, char*, ...);

extern void u_wallops(char*, ...);

extern int init_sendto(void);

#endif

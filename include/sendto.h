/* ircd-micro, sendto.h -- unified message sending
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_SENDTO_H__
#define __INC_SENDTO_H__

#define ST_ALL        0xffffffff

#define ST_SERVERS             1
#define ST_USERS               2

/* second argument is excluded from message */
extern void u_sendto_chan(A5(u_chan*, u_conn*, uint, char*, ...));

/* sends to all connections a user is visible to */
extern void u_sendto_visible(A4(u_user*, uint, char*, ...));

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

extern void u_roster_add(); /* unsigned char r, u_conn* */
extern void u_roster_del(); /* unsigned char r, u_conn* */
extern void u_roster_del_all(); /* u_conn* */
extern void u_roster_f(A4(unsigned char, u_conn*, char*, ...));

extern void u_wallops(A2(char*, ...));

extern int init_sendto();

#endif

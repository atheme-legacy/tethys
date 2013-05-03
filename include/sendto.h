/* ircd-micro, sendto.h -- unified message sending
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_SENDTO_H__
#define __INC_SENDTO_H__

/* second argument is excluded from message */
extern void u_sendto_chan(A4(u_chan*, u_conn*, char*, ...));

/* sends to all connections a user is visible to */
extern void u_sendto_visible(A3(u_user*, char*, ...));

/*
      +-----------------------------------------------+
   00 |XX|+w|                                         |
      +--+--+          SIMPLE ROSTERS                 |
      |                                               | 1f
      +-----------------------------------------------+
      /                   (UNUSED)                    /
      +-----------------------------------------------+
   c0 |                                               |
      |                   SNOMASKS                    |
      |                                               | ff
      +-----------------------------------------------+
 */
#define R_WALLOPS      1
#define R_SNOTICE(c)   (0xc0 | ((c) & 0x3f))

extern void u_roster_add(); /* unsigned char r, u_user_local* */
extern void u_roster_del(); /* unsigned char r, u_user_local* */
extern void u_roster_f(A3(unsigned char, char*, ...));

extern int init_sendto();

#endif

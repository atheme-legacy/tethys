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

extern int init_sendto();

#endif

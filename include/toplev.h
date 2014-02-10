/* Tethys, toplev.h -- toplevel IRC connection operations
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_TOPLEV_H__
#define __INC_TOPLEV_H__

#include "conn.h"

typedef struct u_toplev_origin u_toplev_origin;

struct u_toplev_origin {
	mowgli_eventloop_pollable_t *poll;
	mowgli_node_t n;
};

extern void u_toplev_attach(u_conn*);

extern u_toplev_origin *u_toplev_origin_create(mowgli_eventloop_t*, ulong addr,
                                               ushort port);
extern void u_toplev_origin_destroy(u_toplev_origin *orig);

extern int init_toplev(void);

#endif

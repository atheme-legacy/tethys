/* Tethys, server.h -- server data
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_SERVER_H__
#define __INC_SERVER_H__

#define MAXSERVNAME  40
#define MAXSERVDESC  80
#define MAXNETNAME   30
#define MAXADMIN    512

#define SERVER_OBUFSIZE 262144

#define CAPAB_QS              0x1
#define CAPAB_EX              0x2
#define CAPAB_CHW             0x4
#define CAPAB_IE              0x8
#define CAPAB_EOB            0x10
#define CAPAB_KLN            0x20
#define CAPAB_UNKLN          0x40
#define CAPAB_KNOCK          0x80
#define CAPAB_TB            0x100
#define CAPAB_ENCAP         0x200
#define CAPAB_SERVICES      0x400
#define CAPAB_SAVE          0x800
#define CAPAB_RSFNC        0x1000
#define CAPAB_EUID         0x2000
#define CAPAB_CLUSTER      0x4000

#define SERVER_IS_BURSTING    0x1

/* registration postpone */
#define SERVER_MASK_WAIT        0xff000000

typedef struct u_server u_server;

#include "conn.h"

struct u_server {
	u_conn *link; /* only NULL for &me */
	ulong flags;

	char sid[4]; /* if empty, this server is a TS5 */
	char name[MAXSERVNAME+1];
	char desc[MAXSERVDESC+1];
	uint capab;

	uint hops;
	u_server *parent;

	/* statistics */
	uint nusers;
	uint nlinks;
};

#define IS_SERVER_LOCAL(sv) ((sv)->hops == 1)

#define SERVER(sv) ((u_server*)(sv))

extern mowgli_patricia_t *servers_by_sid;
extern mowgli_patricia_t *servers_by_name;

extern u_server me;
extern mowgli_list_t my_motd;
extern mowgli_list_t my_admininfo;
extern char my_net_name[MAXNETNAME+1];

extern u_server *u_server_by_sid(char *sid);
extern u_server *u_server_by_name(char *name);
extern u_server *u_server_find(char *str);

static inline u_server *u_server_by_ref(u_conn *conn, char *ref)
{
	if (!ref) return NULL;
	return (conn && conn->ctx == CTX_SERVER && isdigit(*ref)) ?
	        u_server_by_sid(ref) : u_server_by_name(ref);
}

extern void u_server_add_capabs(u_server*, char *caps);
extern void u_my_capabs(char *buf);

extern void u_server_make_sreg(u_conn*, char *sid);
extern u_server *u_server_new_remote(u_server *parent, char *sid,
                                     char *name, char *desc);
extern void u_server_unlink(u_server*);

extern void u_server_burst_1(u_server*, u_link*);
extern void u_server_burst_2(u_server*, u_link*);
extern void u_server_eob(u_server*);

extern int init_server(void);

#endif

/* ircd-micro, server.h -- server data
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_SERVER_H__
#define __INC_SERVER_H__

#define MAXSERVNAME 40
#define MAXSERVDESC 80
#define MAXNETNAME 30

struct u_server {
	struct u_conn *conn;
	char sid[4];
	char name[MAXSERVNAME+1];
	char desc[MAXSERVDESC+1];
};

extern struct u_server me;
extern struct u_list my_motd;
extern char my_net_name[MAXNETNAME+1];

extern int init_server();

#endif

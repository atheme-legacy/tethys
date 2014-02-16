/* Tethys, auth.h -- authentication management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_AUTH_H__
#define __INC_AUTH_H__

#define MAXCLASSNAME 64
#define MAXAUTHNAME 64
#define MAXPASSWORD 256
#define MAXOPERNAME 64
#define MAXSERVERNAME 128

typedef struct u_class u_class;
typedef struct u_auth_block u_auth_block;
typedef struct u_oper_block u_oper_block;
typedef struct u_link_block u_link_block;

#include "conn.h"
#include "util.h"
#include "server.h"

struct u_class {
	char name[MAXCLASSNAME+1];
	int timeout;
};

struct u_auth_block {
	char name[MAXAUTHNAME+1];
	char classname[MAXCLASSNAME+1];
	u_class *cls;
	u_cidr cidr;
	char pass[MAXPASSWORD+1];
	mowgli_node_t n;
};

struct u_oper_block {
	char name[MAXOPERNAME+1];
	char pass[MAXPASSWORD+1];
	char authname[MAXAUTHNAME+1];
	u_auth_block *auth;
};

struct u_link_block {
	char name[MAXSERVERNAME+1];
	char host[INET_ADDRSTRLEN];
	char recvpass[MAXPASSWORD+1];
	char sendpass[MAXPASSWORD+1];
	char classname[MAXCLASSNAME+1];
	u_class *cls;
	mowgli_node_t n;
};

extern u_map *all_classes;
extern u_map *all_auths;
extern u_map *all_opers;
extern u_map *all_links;

extern u_auth_block *u_find_auth(u_conn*);
extern u_oper_block *u_find_oper(u_auth_block*, char*, char*);
extern u_link_block *u_find_link(u_server*);

extern int init_auth(void);

#endif

/* ircd-micro, msg.h -- IRC messages and commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_MSG_H__
#define __INC_MSG_H__

/* from jilles' ts6.txt */
#define U_MSG_MAXARGS 15

typedef struct u_msg u_msg;

struct u_msg {
	u_entity *src;
	char *srcstr;
	char *command;
	char *argv[U_MSG_MAXARGS];
	int argc;
};

/* the parser will modify the string */
extern int u_msg_parse(); /* u_msg*, char* */

#define MAXCOMMANDLEN 16

typedef struct u_cmd u_cmd;

/* use -1 for ctx to invoke command in all contexes */
struct u_cmd {
	char name[MAXCOMMANDLEN+1];
	int ctx;
	void (*cb)(); /* u_conn* src, u_msg* */
	int nargs;
};

extern int u_cmds_reg(); /* u_cmd*, terminated with empty name */
extern void u_cmd_invoke(); /* u_conn*, u_msg* */

extern int init_cmd();

#endif

#ifndef __INC_MSG_H__
#define __INC_MSG_H__

/* from jilles' ts6.txt */
#define U_MSG_MAXARGS 15

struct u_msg {
	char *source;
	char *command;
	char *argv[U_MSG_MAXARGS];
	int argc;
};

/* the parser will modify the string */
extern int u_msg_parse(); /* u_msg*, char* */

#define MAXCOMMANDLEN 16

/* use -1 for ctx to invoke command in all contexes */
struct u_cmd {
	char name[MAXCOMMANDLEN+1];
	int ctx;
	void (*cb)(); /* u_conn* src, u_msg* */
	int nargs;
};

extern int u_cmds_reg(); /* struct u_cmd*, terminated with empty name */
extern void u_cmd_invoke(); /* u_conn*, u_msg* */

#endif

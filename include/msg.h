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
extern int u_msg_fmt(); /* char *dst, char *fmt, ... */

/*
   A little note on u_msg_fmt() format strings:

   They look like ":ssd:" and are designed to make the message
   formatting system as abstract as possible. Each character in
   the format corresponds to an argument. The character
   specifies how the argument should be treated, and how it
   should be added to the destination string:
     's' -- The parameter is a string and should be inserted
            literally.
     ':' -- Like 's', except the parameter should be preceeded
            by a colon. This is only valid at the beginning or the
            end of the format! The corresponding format argument
            may not have spaces if it is to be placed at the
            beginning of the output string.
     'c' -- A character. Should be inserted literally.
     'd' -- A decimal integer.
     'n' -- An IRC numeric. These are decimal values and are always
            represented as a series of 3 decimal characters, padded with
            zeros as necessary.
   Here are some example invocations and their outputs:
     u_msg_fmt(dst, "ss:", "NOTICE", "aji", "This is a notice");
        => NOTICE aji :This is a notice"
     u_msg_fmt(dst, ":ns:", "my.host", 1, "aji", "Welcome to the net!");
        => :my.host 001 aji :Welcome to the net!
     u_msg_fmt(dst, "s:", "PING", "my.host");
        => PING :my.host
   This is very simple stuff people. Don't make it more complicated
   than it needs to be, kthx.
 */

#define MAXCOMMANDLEN 16

struct u_cmd {
	char name[MAXCOMMANDLEN+1];
	/* cc=unregged connection, cs=server, cu=user */
	void (*cc)(); /* u_connection* src, u_msg* */
	int ncc;
	void (*cu)(); /* u_user* src, u_msg* */
	int ncu;
	void (*cs)(); /* u_server* src, u_msg* */
	int ncs;
};

extern int u_cmds_reg(); /* struct u_cmd*, terminated with empty name */
extern void u_cmds_unreg(); /* struct u_cmd*, terminated as above */

extern void u_cmd_invoke_c(); /* u_connection*, u_msg* */
extern void u_cmd_invoke_u(); /* u_user*, u_msg* */
extern void u_cmd_invoke_s(); /* u_server*, u_msg* */

#endif

/* Tethys, msg.h -- IRC messages and commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_MSG_H__
#define __INC_MSG_H__

typedef struct u_msg u_msg;
typedef struct u_cmd u_cmd;
typedef struct u_sourceinfo u_sourceinfo;

#include "module.h"

/* from jilles' ts6.txt */
#define U_MSG_MAXARGS 15

#define MSG_REPEAT 0x0001

struct u_msg {
	char *srcstr;

	char *command;

	char *argv[U_MSG_MAXARGS];
	int argc;

	ulong flags;
	char *propagate;
};

/* the parser will modify the string */
extern int u_msg_parse(u_msg*, char*);

/* source mask bits */
#define SRC_LOCAL_OPER           0x00000001 /* OPER, LOCAL_USER, USER, ANY */
#define SRC_REMOTE_OPER          0x00000002 /* OPER, REMOTE_USER, USER, ANY */
#define SRC_LOCAL_UNPRIVILEGED   0x00000004 /* UNPRIV, LOCAL_USER, USER, ANY */
#define SRC_REMOTE_UNPRIVILEGED  0x00000008 /* UNPRIV, REMOTE_USER, USER, ANY */
#define SRC_LOCAL_SERVER         0x00000010 /* SERVER, ANY */
#define SRC_REMOTE_SERVER        0x00000020 /* SERVER, ANY */
#define SRC_ENCAP_USER           0x00000040 /* ENCAP */
#define SRC_ENCAP_SERVER         0x00000080 /* ENCAP */
#define SRC_UNREGISTERED_USER    0x00000100 /* UNREGISTERED */
#define SRC_UNREGISTERED_SERVER  0x00000200 /* UNREGISTERED */
#define SRC_FIRST                0x00000400 /* UNREGISTERED */
#define SRC_OTHER                0x80000000 /* for possible extensions? */

#define SRC_OPER         (SRC_LOCAL_OPER | SRC_REMOTE_OPER)
#define SRC_UNPRIVILEGED (SRC_LOCAL_UNPRIVILEGED | SRC_REMOTE_UNPRIVILEGED)
#define SRC_LOCAL_USER   (SRC_LOCAL_OPER | SRC_LOCAL_UNPRIVILEGED)
#define SRC_REMOTE_USER  (SRC_REMOTE_OPER | SRC_REMOTE_UNPRIVILEGED)
#define SRC_USER         (SRC_LOCAL_USER | SRC_REMOTE_USER)
#define SRC_SERVER       (SRC_LOCAL_SERVER | SRC_REMOTE_SERVER)
#define SRC_ANY          (SRC_USER | SRC_SERVER)
#define SRC_ENCAP        (SRC_ENCAP_USER | SRC_ENCAP_SERVER)
#define SRC_UNREGISTERED (SRC_UNREGISTERED_USER | SRC_UNREGISTERED_SERVER \
                          | SRC_FIRST)
#define SRC_C2S (SRC_LOCAL_USER)
#define SRC_S2S (SRC_REMOTE_USER | SRC_SERVER)

#include "conn.h"
#include "user.h"
#include "server.h"
#include "ratelimit.h"

/* Any pointer fields can be NULL. */
struct u_sourceinfo {
	/* source = connection message came from,
	   link = link to reach source. should be equal to source.
	   local = local link for source, if any */
	u_conn *source;
	u_conn *link;
	u_conn *local;

	/* source mask bits */
	ulong mask;

	/* name and id of the source */
	const char *name;
	const char *id;

	/* user and server pointers. only one is non-NULL */
	u_user *u;
	u_server *s;
};

#define SRC_HAS_BITS(si, bits) (((si)->mask & (bits)) != 0)

#define SRC_IS_OPER(si) SRC_HAS_BITS(si, SRC_OPER)
#define SRC_IS_USER(si) SRC_HAS_BITS(si, SRC_USER)
#define SRC_IS_SERVER(si) SRC_HAS_BITS(si, SRC_SERVER)
#define SRC_IS_LOCAL_USER(si) SRC_HAS_BITS(si, SRC_LOCAL_USER)

extern int u_src_num(u_sourceinfo *si, int num, ...);
extern void u_src_f(u_sourceinfo *si, const char *fmt, ...);

#define MAXCOMMANDLEN 16

/* command flags */
#define CMD_PROP_MASK          0x0003
#define CMD_PROP_NONE          0x0000
#define CMD_PROP_BROADCAST     0x0001
#define CMD_PROP_ONE_TO_ONE    0x0002
#define CMD_PROP_HUNTED        0x0003

#define CMD_DO_BROADCAST ((void*)1)

struct u_cmd {
	char name[MAXCOMMANDLEN+1];
	/* The 'mask' field here specifies which types of source to
	   match. All commands with a matching name are iterated over,
	   and the first one matching the source is executed. If no
	   command has the specified bit, then information about what
	   bits the command masks have collectively can provide insight
	   into the correct error message to send back to the user
	   (instead of just "no such command") */
	ulong mask;
	int (*cb)(u_sourceinfo *si, u_msg*);

	int nargs;

	ulong flags;

	u_ratelimit_cmd_t rate;

	/* users should not initialize the rest of this struct to
	   anything */
	u_module *owner;
	bool loaded;
	struct u_cmd *next, *prev;
	int runs, usecs;
};

extern mowgli_patricia_t *all_commands;

extern int u_cmds_reg(u_cmd*); /* terminated with empty name */
extern int u_cmd_reg(u_cmd*); /* single command */
extern void u_cmd_unreg(u_cmd*);
extern void u_cmd_invoke(u_conn*, u_msg*, char *line);

extern int u_repeat_as_user(u_sourceinfo *si, u_msg *msg);
extern int u_repeat_as_server(u_sourceinfo *si, u_msg *msg, char *sid);

extern int init_cmd(void);

#endif

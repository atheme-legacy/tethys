/* ircd-micro, msg.c -- IRC messages and commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

char *ws_skip(s) char *s;
{
	while (*s && isspace(*s))
		s++;
	return s;
}

char *ws_cut(s) char *s;
{
	while (*s && !isspace(*s))
		s++;
	if (!*s) return s;
	*s++ = '\0';
	return ws_skip(s);
}

int u_msg_parse(msg, s) u_msg *msg; char *s;
{
	int i;
	s = ws_skip(s);
	if (!*s) return -1;

	if (*s == ':') {
		msg->srcstr = ++s;
		s = ws_cut(s);
		if (!*s) return -1;
	} else {
		msg->srcstr = NULL;
	}

	msg->command = s;
	s = ws_cut(s);

	for (i=0; i<U_MSG_MAXARGS; i++)
		msg->argv[i] = NULL;

	for (msg->argc=0; msg->argc<U_MSG_MAXARGS && *s;) {
		if (*s == ':') {
			msg->argv[msg->argc++] = ++s;
			break;
		}

		msg->argv[msg->argc++] = s;
		s = ws_cut(s);
	}

	ascii_canonize(msg->command);

	return 0;
}


static u_trie *commands[CTX_MAX];

int reg_one_real(cmd, ctx) u_cmd *cmd;
{
	if (u_trie_get(commands[ctx], cmd->name))
		return -1;

	u_trie_set(commands[ctx], cmd->name, cmd);
	return 0;
}

int reg_one(cmd) u_cmd *cmd;
{
	int i, err;

	if (cmd->ctx >= 0)
		return reg_one_real(cmd, cmd->ctx);

	for (i=0; i<CTX_MAX; i++) {
		if ((err = reg_one_real(cmd, i)) < 0)
			return err;
	}
	return 0;
}

int u_cmds_reg(cmds) u_cmd *cmds;
{
	int err;
	for (; cmds->name[0]; cmds++) {
		if ((err = reg_one(cmds)) < 0)
			return err;
	}
	return 0;
}

void u_cmd_invoke(conn, msg) u_conn *conn; u_msg *msg;
{
	u_cmd *cmd;
	u_entity e;

	cmd = u_trie_get(commands[conn->ctx], msg->command);

	if (!cmd) {
		if (conn->ctx == CTX_USER || conn->ctx == CTX_UREG)
			u_conn_num(conn, ERR_UNKNOWNCOMMAND, msg->command);
		else
			u_log(LG_ERROR, "%G used unknown command %s",
			      conn, msg->command);
		return;
	}

	if (msg->argc < cmd->nargs) {
		if (conn->ctx == CTX_USER)
			u_conn_num(conn, ERR_NEEDMOREPARAMS, msg->command);
		else
			u_log(LG_ERROR, "%G did not provide enough args to %s",
			      conn, msg->command);
		return;
	}

	msg->src = NULL;
	switch (conn->ctx) {
	case CTX_USER:
	case CTX_UREG:
		msg->src = u_entity_from_user(&e, conn->priv);
		break;

	case CTX_SERVER:
	case CTX_SBURST:
	case CTX_SREG:
		if (msg->srcstr)
			msg->src = u_entity_from_id(&e, msg->srcstr);
		else
			msg->src = u_entity_from_server(&e, conn->priv);
		break;
	}

	u_log(LG_FINE, "%E INVOKE %s [%p]", msg->src, cmd->name, cmd->cb);

	cmd->cb(conn, msg);
}

int init_cmd()
{
	int i;

	for (i=0; i<CTX_MAX; i++) {
		if ((commands[i] = u_trie_new(NULL)) == NULL)
			return -1;
	}

	return 0;
}

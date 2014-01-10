/* ircd-micro, msg.c -- IRC messages and commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

char *ws_skip(char *s)
{
	while (*s && isspace(*s))
		s++;
	return s;
}

char *ws_cut(char *s)
{
	while (*s && !isspace(*s))
		s++;
	if (!*s) return s;
	*s++ = '\0';
	return ws_skip(s);
}

int u_msg_parse(u_msg *msg, char *s)
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


static mowgli_patricia_t *commands[CTX_MAX];

static int reg_one_real(u_cmd *cmd, int ctx)
{
	if (mowgli_patricia_retrieve(commands[ctx], cmd->name))
		return -1;

	mowgli_patricia_add(commands[ctx], cmd->name, cmd);
	return 0;
}

static int reg_one(u_cmd *cmd)
{
	int i, err;

	if (cmd->owner != NULL)
		return -1;
	cmd->owner = u_module_loading();

	if (cmd->ctx >= 0)
		return reg_one_real(cmd, cmd->ctx);

	for (i=0; i<CTX_MAX; i++) {
		if ((err = reg_one_real(cmd, i)) < 0)
			return err;
	}

	return 0;
}

int u_cmds_reg(u_cmd *cmds)
{
	int err;
	for (; cmds->name[0]; cmds++) {
		if ((err = reg_one(cmds)) < 0)
			return err;
	}
	return 0;
}

int u_cmd_reg(u_cmd *cmd)
{
	return reg_one(cmd);
}

static void *on_module_unload(void *unused, void *m)
{
	mowgli_patricia_iteration_state_t state;
	u_cmd *cmd;
	int i;

	for (i=0; i<CTX_MAX; i++) {
		MOWGLI_PATRICIA_FOREACH(cmd, &state, commands[i]) {
			if (cmd->owner != m)
				continue;

			mowgli_patricia_delete(commands[i], cmd->name);
		}
	}

	return NULL;
}

void u_cmd_invoke(u_conn *conn, u_msg *msg)
{
	u_cmd *cmd;
	u_entity e;

	cmd = mowgli_patricia_retrieve(commands[conn->ctx], msg->command);

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
	case CTX_SREG:
		if (msg->srcstr)
			msg->src = u_entity_from_ref(&e, msg->srcstr);
		else
			msg->src = u_entity_from_server(&e, conn->priv);
		break;
	}

	u_log(LG_FINE, "%E INVOKE %s [%p]", msg->src, cmd->name, cmd->cb);

	cmd->cb(conn, msg);
}

int init_cmd(void)
{
	int i;

	u_hook_add(HOOK_MODULE_UNLOAD, on_module_unload, NULL);

	for (i=0; i<CTX_MAX; i++) {
		if ((commands[i] = mowgli_patricia_create(NULL)) == NULL)
			return -1;
	}

	return 0;
}

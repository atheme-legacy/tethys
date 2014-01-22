/* Tethys, msg.c -- IRC messages and commands
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


static mowgli_patricia_t *commands;

static int reg_one(u_cmd *cmd)
{
	u_cmd *at;

	u_log(LG_DEBUG, "Registering command %s", cmd->name);

	if (cmd->loaded) {
		u_log(LG_ERROR, "Command %s already registered!", cmd->name);
		return -1;
	}
	cmd->loaded = true;

	cmd->owner = u_module_loading();

	/* TODO: check mutual exclusivity */

	at = mowgli_patricia_retrieve(commands, cmd->name);
	cmd->next = at;
	cmd->prev = NULL;
	if (at != NULL)
		at->prev = cmd;
	mowgli_patricia_add(commands, cmd->name, cmd);

	return 0;
}

int u_cmds_reg(u_cmd *cmds)
{
	int err = 0;
	for (; cmds->name[0]; cmds++)
		err += reg_one(cmds); /* is this ok? */
	return err;
}

int u_cmd_reg(u_cmd *cmd)
{
	return reg_one(cmd);
}

void u_cmd_unreg(u_cmd *cmd)
{
	if (cmd->next)
		cmd->next->prev = cmd->prev;
	if (cmd->prev)
		cmd->prev->next = cmd->next;

	if (cmd->prev == NULL) {
		if (cmd->next == NULL) {
			mowgli_patricia_delete(commands, cmd->name);
		} else {
			mowgli_patricia_add(commands, cmd->name, cmd->next);
		}
	}
}

static void *on_module_unload(void *unused, void *m)
{
	mowgli_patricia_iteration_state_t state;
	u_cmd *cmd, *next;

	MOWGLI_PATRICIA_FOREACH(cmd, &state, commands) {
		for (; cmd; cmd = next) {
			next = cmd->next;
			if (cmd->owner != m)
				continue;
			u_cmd_unreg(cmd);
		}
	}

	return NULL;
}

void u_cmd_invoke(u_conn *conn, u_msg *msg, char *line)
{
	u_cmd *cmd;
	u_sourceinfo si;

	cmd = mowgli_patricia_retrieve(commands, msg->command);

	/* TODO */

	u_conn_num(conn, ERR_UNKNOWNCOMMAND, msg->command);

	return;

	u_log(LG_FINE, "%s INVOKE %s [%p]", msg->srcstr, cmd->name, cmd->cb);

	msg->propagate = NULL;

	cmd->cb(&si, msg);

	if (msg->propagate && cmd->propagation && conn->ctx == CTX_SERVER) {
		switch (cmd->propagation) {
		case CMD_PROP_NONE:
			break;

		case CMD_PROP_BROADCAST:
			u_roster_f(R_SERVERS, conn, "%s", line);
			break;

		case CMD_PROP_ONE_TO_ONE:
			/* TODO: fix this when we decide what to do with u_entity
			if (u_entity_from_ref(&e, msg->propagate) && e.link)
				u_conn_f(e.link, "%s", line); */
			break;

		case CMD_PROP_HUNTED:
			u_log(LG_WARN, "%s uses unimplemented propagation "
			      "type CMD_PROP_HUNTED");
			break;

		default:
			u_log(LG_WARN, "%s has unknown propagation type %d",
			      cmd->name, cmd->propagation);
			break;
		}
	}
}

int init_cmd(void)
{
	u_hook_add(HOOK_MODULE_UNLOAD, on_module_unload, NULL);

	if ((commands = mowgli_patricia_create(NULL)) == NULL)
		return -1;

	return 0;
}

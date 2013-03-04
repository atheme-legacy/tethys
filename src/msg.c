#include "ircd.h"

char *ws_skip(s)
char *s;
{
	while (*s && isspace(*s))
		s++;
	return s;
}

char *ws_cut(s)
char *s;
{
	while (*s && !isspace(*s))
		s++;
	if (!*s) return s;
	*s++ = '\0';
	return ws_skip(s);
}

int u_msg_parse(msg, s)
struct u_msg *msg;
char *s;
{
	int i;
	char *p;

	s = ws_skip(s);
	if (!*s) return -1;

	if (*s == ':') {
		msg->source = ++s;
		s = ws_cut(s);
		if (!*s) return -1;
	} else {
		msg->source = NULL;
	}

	msg->command = s;
	s = ws_cut(s);

	for (msg->argc=0; msg->argc<U_MSG_MAXARGS && *s;) {
		if (*s == ':') {
			msg->argv[msg->argc++] = ++s;
			break;
		}

		msg->argv[msg->argc++] = s;
		s = ws_cut(s);
	}

	return 0;
}


/* let bss initialize to zero */
static struct u_hash commands;

static struct u_cmd *find_command(name)
char *name;
{
	/* TODO: uppercase name? */
	return u_hash_get(&commands, name);
}

static int u_cmd_reg_one(cmd)
struct u_cmd *cmd;
{
	if (u_hash_get(&commands, cmd->name))
		return -1;

	u_hash_set(&commands, cmd->name, cmd);
}

int u_cmds_reg(cmds)
struct u_cmd *cmds;
{
	int err;
	for (; cmds->name[0]; cmds++) {
		if ((err = u_cmd_reg_one(cmds)) < 0)
			return err;
	}
	return 0;
}

void u_cmds_unreg(cmds)
struct u_cmd *cmds;
{
	int err;
	for (; cmds->name[0]; cmds++)
		u_hash_del(&commands, cmds->name);
}

void u_cmd_invoke_c(conn, msg)
struct u_connection *conn;
struct u_msg *msg;
{
	struct u_cmd *cmd = find_command(msg->command);

	/* TODO: command not found */
	if (!cmd) return;

	/* TODO: not enough args */
	if (msg->argc < cmd->ncc) return;

	cmd->cc(conn, msg);
}

void u_cmd_invoke_u(user, msg)
struct u_user *user;
struct u_msg *msg;
{
	struct u_cmd *cmd = find_command(msg->command);

	/* TODO: */
	if (!cmd) return;
	if (msg->argc < cmd->ncu) return;

	cmd->cu(user, msg);
}

void u_cmd_invoke_s(serv, msg)
struct u_server *serv;
struct u_msg *msg;
{
	struct u_cmd *cmd = find_command(msg->command);

	/* TODO: */
	if (!cmd) return;
	if (msg->argc < cmd->ncs) return;

	cmd->cs(serv, msg);
}

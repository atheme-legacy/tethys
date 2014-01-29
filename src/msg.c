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


int u_src_num(u_sourceinfo *si, int num, ...)
{
	va_list va;
	const char *tgt;

	if (!si->link) {
		u_log(LG_WARN, "Tried to u_src_num(%I)", si);
		return -1;
	}

	va_start(va, num);

	tgt = si->link->ctx == CTX_SERVER ? si->id : si->name;
	tgt = tgt ? tgt : "*";

	u_conn_vnum(si->link, tgt, num, va);

	va_end(va);

	return -1;
}

void u_src_f(u_sourceinfo *si, const char *fmt, ...)
{
	va_list va;

	if (!si->link) {
		u_log(LG_WARN, "Tried to u_src_f(%I)", si);
		return;
	}

	va_start(va, fmt);
	u_conn_vf(si->link, fmt, va);
	va_end(va);
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
	if (at != NULL) {
		at->prev = cmd;
		mowgli_patricia_delete(commands, cmd->name);
	}
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

	u_log(LG_DEBUG, "Unregistering command %s", cmd->name);

	if (cmd->prev == NULL) {
		mowgli_patricia_delete(commands, cmd->name);
		if (cmd->next != NULL)
			mowgli_patricia_add(commands, cmd->name, cmd->next);
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

static void fill_source_server(u_sourceinfo *si, u_server *s)
{
	si->s = s;
	si->link = si->s->conn;
	si->local = NULL;
	if (IS_SERVER_LOCAL(si->s)) {
		si->local = si->link;
		si->mask &= SRC_LOCAL_SERVER;
	} else {
		si->mask &= SRC_REMOTE_SERVER;
	}
}

static void fill_source_user(u_sourceinfo *si, u_user *u)
{
	si->u = u;
	si->link = u_user_conn(si->u);
	if (IS_LOCAL_USER(si->u)) {
		si->local = si->link;
		si->mask &= SRC_LOCAL_USER;
	} else {
		si->mask &= SRC_REMOTE_USER;
	}
}

static bool fill_source_by_id(u_sourceinfo *si, u_conn *conn, char *src)
{
	int n;

	if (!isdigit(*src))
		return false;

	n = strlen(src);

	switch (n) {
	case 3:
		si->s = u_server_by_sid(src);
		if (si->s == NULL)
			return false;
		fill_source_server(si, si->s);
		break;

	case 9:
		si->u = u_user_by_uid(src);
		if (si->u == NULL)
			return false;
		fill_source_user(si, si->u);
		break;

	default:
		u_log(LG_ERROR, "ID-like source %s is not 3 or 9 chars!", src);
		return false;
	}

	return true;
}

static bool fill_source_by_name(u_sourceinfo *si, u_conn *conn, char *src)
{
	if (strchr(src, '.')) {
		si->s = u_server_by_name(src);
		if (si->s == NULL)
			return false;
		fill_source_server(si, si->s);
	} else {
		si->u = u_user_by_nick(src);
		if (si->u == NULL)
			return false;
		fill_source_user(si, si->u);
	}

	return true;
}

static bool fill_source_by_unk_id(u_sourceinfo *si, u_conn *conn, char *src)
{
	int n;

	/* This function fills a sourceinfo with as much information as
	   possible about a remote source given only a UID. This sort of thing
	   could happen if a remote IRCD is sending messages from a user or
	   server who has not yet registered, as is the case for some ENCAPs

	   As a consequence of this function, a SRC_USER or SRC_SERVER
	   command handler could be invoked which expects si->u or si->s to
	   be non-NULL. At some point, command handlers should be allowed
	   to indicate that a command can come from an unregistered source. */

	if (!isdigit(*src))
		return false;

	n = strlen(src);

	if (n != 3 && n != 9)
		return false;

	si->link = si->source;
	si->local = NULL;
	si->id = src;

	if (n == 9)
		si->mask &= SRC_REMOTE_UNPRIVILEGED;
	else if (n == 3)
		si->mask &= SRC_REMOTE_SERVER;

	return true;
}

static void fill_source(u_sourceinfo *si, u_conn *conn, u_msg *msg)
{
	memset(si, 0, sizeof(*si));

	si->source = conn;

	si->mask = (ulong) -1; /* all 1's */

	if (!(conn->flags & U_CONN_REGISTERED)) {
		si->link = si->local = conn;

		switch (conn->ctx) {
		case CTX_NONE:
			si->mask &= SRC_FIRST;
			break;
		case CTX_USER:
			si->mask &= SRC_UNREGISTERED_USER;
			si->u = conn->priv;
			si->id = si->u->uid;
			break;
		case CTX_SERVER:
			si->mask &= SRC_UNREGISTERED_SERVER;
			si->s = conn->priv;
			si->id = si->s->sid;
			break;
		default:
			u_log(LG_SEVERE, "fill_source inconsistency");
			abort();
		}

		return;
	}

	switch (conn->ctx) {
	case CTX_USER:
		si->mask &= SRC_LOCAL_USER;
		si->u = conn->priv;
		si->link = si->local = conn;
		break;

	case CTX_SERVER:
		if (!msg->srcstr || !*msg->srcstr) {
			si->mask &= SRC_LOCAL_SERVER;
			si->s = conn->priv;
			si->link = si->local = conn;
			break;
		}

		if (!fill_source_by_id(si, conn, msg->srcstr) &&
		    !fill_source_by_name(si, conn, msg->srcstr) &&
		    !fill_source_by_unk_id(si, conn, msg->srcstr)) {
			u_log(LG_WARN, "%G sent source we can't use: %s",
			      conn->priv, msg->srcstr);
		}
		break;

	default:
		u_log(LG_SEVERE, "fill_source inconsistency");
		abort();
	}

	if (si->u && si->s) {
		u_log(LG_SEVERE, "fill_source inconsistency");
		abort();
	}

	if (si->u) {
		si->name = si->u->nick;
		si->id = si->u->uid;
	}

	if (si->s) {
		si->name = si->s->name;
		si->id = si->s->sid;
	}

	if (SRC_IS_USER(si)) {
		if (!si->u || !(si->u->flags & UMODE_OPER))
			si->mask &= SRC_UNPRIVILEGED;
		else
			si->mask &= SRC_OPER;
	}
}

static u_cmd *find_command(const char *command, ulong mask, ulong *bits_tested)
{
	u_cmd *cmd;

	*bits_tested = 0;

	/* map numerics to ### */
	if (!strcmp(command, "###"))
		return NULL;
	if (isdigit(command[0]) && isdigit(command[1])
	    && isdigit(command[2]) && !command[3])
		command = "###";

	cmd = mowgli_patricia_retrieve(commands, command);

	for (; cmd; cmd = cmd->next) {
		*bits_tested |= cmd->mask;
		if ((cmd->mask & mask) != 0)
			break;
	}

	return cmd;
}

static void report_failure(u_sourceinfo *si, u_msg *msg, ulong bits_tested)
{
	/* TODO: make this function smarter */

	if (si->source && si->source->ctx == CTX_SERVER) {
		u_log(LG_SEVERE, "%G invocation of %s failed!",
		      si->source, msg->command);
		return;
	}

	if (bits_tested == 0) {
		u_conn_num(si->source, ERR_UNKNOWNCOMMAND, msg->command);
		return;
	}

	if ((bits_tested & SRC_USER) && !(si->mask & SRC_USER)) {
		u_conn_num(si->source, ERR_NOTREGISTERED);
		return;
	}

	if ((bits_tested & SRC_UNREGISTERED) && !(si->mask & SRC_UNREGISTERED)) {
		u_conn_num(si->source, ERR_ALREADYREGISTERED);
		return;
	}

	if ((bits_tested & SRC_OPER) && !(si->mask & SRC_OPER)) {
		u_conn_num(si->source, ERR_NOPRIVILEGES);
		return;
	}

	u_conn_num(si->source, ERR_UNKNOWNCOMMAND, msg->command);
}

static void propagate_message(u_sourceinfo *si, u_msg *msg, u_cmd *cmd, char *line)
{
	switch (cmd->propagation) {
	case CMD_PROP_NONE:
		break;

	case CMD_PROP_BROADCAST:
		u_sendto_servers(si->source, "%s", line);
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

void u_cmd_invoke(u_conn *conn, u_msg *msg, char *line)
{
	u_cmd *cmd, *last_cmd;
	u_sourceinfo si;
	ulong bits_tested;

	last_cmd = NULL;

again:
	fill_source(&si, conn, msg);
	u_log(LG_FINE, "source mask = 0x%x", si.mask);

	if (!(cmd = find_command(msg->command, si.mask, &bits_tested))) {
		report_failure(&si, msg, bits_tested);
		return;
	}

	u_log(LG_FINE, "%s INVOKE %s [%p]", msg->srcstr, cmd->name, cmd->cb);
	if (cmd == last_cmd) {
		u_log(LG_SEVERE, "command %s requested repeat to itself!",
		      cmd->name);
		abort();
	}
	last_cmd = cmd;

	if (cmd->nargs && msg->argc < cmd->nargs) {
		u_conn_num(conn, ERR_NEEDMOREPARAMS, cmd->name);
		return;
	}

	msg->flags = 0;
	msg->propagate = NULL;

	cmd->cb(&si, msg);

	if (msg->propagate && cmd->propagation && conn->ctx == CTX_SERVER)
		propagate_message(&si, msg, cmd, line);

	if (msg->flags & MSG_REPEAT)
		goto again;
}

int u_repeat_as_user(u_sourceinfo *si, u_msg *msg)
{
	if (!(si->mask & SRC_FIRST)) {
		u_log(LG_SEVERE, "u_repeat_as_* called outside of SRC_FIRST!");
		abort();
	}

	u_user_create_local(si->source);
	msg->flags |= MSG_REPEAT;

	return 0;
}

int u_repeat_as_server(u_sourceinfo *si, u_msg *msg, char *sid)
{
	if (!(si->mask & SRC_FIRST)) {
		u_log(LG_SEVERE, "u_repeat_as_* called outside of SRC_FIRST!");
		abort();
	}

	u_server_make_sreg(si->source, sid);
	msg->flags |= MSG_REPEAT;

	return 0;
}

int init_cmd(void)
{
	u_hook_add(HOOK_MODULE_UNLOAD, on_module_unload, NULL);

	if ((commands = mowgli_patricia_create(NULL)) == NULL)
		return -1;

	return 0;
}

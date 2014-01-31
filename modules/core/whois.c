/* Tethys, core/whois -- WHOIS command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root. */

#include "ircd.h"

static int c_u_whois(u_sourceinfo *si, u_msg *msg)
{
	char *nick, *s;
	u_user *tu;
	u_server *sv;
	bool long_whois = false;
	u_map_each_state state;
	u_chan *c; u_chanuser *cu;
	char buf[512];
	uint w;

	nick = msg->argv[msg->argc - 1];
	if ((s = strchr(nick, ','))) /* cut at ',' idk why */
		*s = '\0';

	if (!(tu = u_user_by_nick(nick)))
		return u_src_num(si, ERR_NOSUCHNICK, nick);


	/* check hunted */

	sv = NULL;
	if (msg->argc > 1) {
		char *server = msg->argv[0];
		long_whois = true;
		if (!(sv = u_server_by_ref(si->source, server))
		    && irccmp(server, nick)) {
			u_src_num(si, ERR_NOSUCHSERVER, server);
			return 0;
		}
	}

	if (sv != NULL && sv != &me) {
		u_conn_f(sv->conn, ":%I WHOIS %S %s", si, sv, nick);
		return 0;
	}


	/* perform whois */

	sv = u_user_server(tu);
	u_src_num(si, RPL_WHOISUSER, tu->nick, tu->ident, tu->host, tu->gecos);


	s = buf;
	w = 512 - MAXSERVNAME - MAXNICKLEN - MAXNICKLEN - 9; /* ??? */

	U_MAP_EACH(&state, tu->channels, &c, &cu) {
		char *p, cbuf[MAXCHANNAME+3];
		int retrying = 0;

		if (c->mode & (CMODE_PRIVATE | CMODE_SECRET)
		    && !u_chan_user_find(c, si->u))
			continue;

		p = cbuf;
		if (cu->flags & CU_PFX_OP)
			*p++ = '@';
		if (cu->flags & CU_PFX_VOICE)
			*p++ = '+';
		strcpy(p, c->name);

	try_again:
		if (!wrap(buf, &s, w, cbuf)) {
			if (retrying) {
				u_log(LG_SEVERE, "Can't fit %s into %s!",
				      cbuf, "RPL_WHOISCHANNELS");
				continue;
			}

			u_src_num(si, RPL_WHOISCHANNELS, tu->nick, buf);
			retrying = 1;
			goto try_again;
		}
	}

	if (s != buf) /* leftovers */
		u_src_num(si, RPL_WHOISCHANNELS, tu->nick, buf);


	u_src_num(si, RPL_WHOISSERVER, tu->nick, sv->name, sv->desc);

	if (tu->away[0])
		u_src_num(si, RPL_AWAY, tu->nick, tu->away);

	if (tu->flags & UMODE_SERVICE)
		u_src_num(si, RPL_WHOISOPERATOR, tu->nick, "a Network Service");
	else if (tu->flags & UMODE_OPER)
		u_src_num(si, RPL_WHOISOPERATOR, tu->nick, "an IRC operator");

	/* TODO: use long_whois */

	u_src_num(si, RPL_ENDOFWHOIS, tu->nick);

	return 0;
}

static u_cmd whois_cmdtab[] = {
	{ "WHOIS", SRC_USER, c_u_whois, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/whois", "Alex Iadicicco", "WHOIS command",
	NULL, NULL, whois_cmdtab);

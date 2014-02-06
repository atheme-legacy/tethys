/* Tethys, mode.c -- mode processing
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static u_mode_info *find_info(u_mode_info *info, char ch)
{
	info += ((unsigned) ch) & 0x7f; /* jump to offset in table */
	return info->ch ? info : NULL;
}

static bool can_set_oper_only(u_sourceinfo *si)
{
	return SRC_HAS_BITS(si, SRC_LOCAL_OPER | SRC_REMOTE_USER | SRC_SERVER);
}

/* foo -> foo!*@*, aji@ -> *!aji@*, etc */
static char *fix_hostmask(char *mask)
{
	static char buf[512];
	char *nick, *ident, *host, *ex, *at;

	if (*mask == '$') {
		/* extbans are untouched */
		strcpy(buf, mask);
		return buf;
	}

	nick = ident = host = "*";

	ex = strchr(mask, '!');
	at = strchr(mask, '@');

	if (ex) *ex++ = '\0';
	if (at) *at++ = '\0';

	if (!ex && !at) { /* foo */
		nick = mask;
	} else if (!ex) { /* foo@bar, @bar, foo@ */
		ident = mask;
		host = at;
	} else if (!at) { /* aji!ex, aji!, !ex */
		nick = mask;
		ident = ex;
	} else { /* aji!i@host, !@host, etc. */
		nick = mask;
		ident = ex;
		host = at;
	}

	if (!*nick) nick = "*";
	if (!*ident) ident = "*";
	if (!*host) host = "*";

	snprintf(buf, 512, "%s!%s@%s", nick, ident, host);
	return buf;
}

static int do_mode_status(u_modes *m, int on, char *param)
{
	void *tgt;

	if (!m->access) {
		m->errors |= MODE_ERR_NO_ACCESS;
		return 1;
	}

	if (param == NULL) {
		m->errors |= MODE_ERR_MISSING_PARAM;
		return 1;
	}

	if (!m->ctx->get_status_target
	    || !(tgt = m->ctx->get_status_target(m, param)))
		return 1;

	if (on) {
		if (m->stacker && m->stacker->put_status)
			m->stacker->put_status(m, 1, tgt);

		if (m->ctx->set_status_bits)
			m->ctx->set_status_bits(m, tgt,
			    m->info->arg.data);
	} else {
		if (m->stacker && m->stacker->put_status)
			m->stacker->put_status(m, 0, tgt);

		if (m->ctx->reset_status_bits)
			m->ctx->reset_status_bits(m, tgt,
			    m->info->arg.data);
	}

	u_log(LG_INFO, "%I %s status on %s", m->setter,
	      on ? "set" : "cleared", param);

	return 1;
}

static int do_mode_flag(u_modes *m, int on)
{
	ulong flags = 0, flag;

	if (!m->access) {
		m->errors |= MODE_ERR_NO_ACCESS;
		return 0;
	}

	if (m->ctx->get_flag_bits)
		flags = m->ctx->get_flag_bits(m);
	flag = m->info->arg.data;

	if (on && !(flags & flag)) {
		if (m->stacker && m->stacker->put_flag)
			m->stacker->put_flag(m, 1);

		if (m->ctx->set_flag_bits)
			m->ctx->set_flag_bits(m, flag);

	} else if (!on && (flags & flag)) {
		if (m->stacker && m->stacker->put_flag)
			m->stacker->put_flag(m, 0);

		if (m->ctx->reset_flag_bits)
			m->ctx->reset_flag_bits(m, flag);
	}

	u_log(LG_INFO, "%I %s 0x%x", m->setter,
	      on ? "set" : "cleared", flag);

	return 0;
}

static int do_mode_list(u_modes *m, int on, char *param)
{
	mowgli_list_t *list;
	mowgli_node_t *n;
	u_listent *ban;
	char *mask;

	if (!param) {
		if (m->stacker && m->stacker->send_list)
			m->stacker->send_list(m);
		return 0;
	}

	if (!m->access) {
		m->errors |= MODE_ERR_NO_ACCESS;
		return 1;
	}

	if (!m->ctx->get_list) {
		u_log(LG_SEVERE, "List mode without list support in context!");
		return 1;
	}

	list = m->ctx->get_list(m, m->info);
	mask = fix_hostmask(param);

	MOWGLI_LIST_FOREACH(n, list->head) {
		ban = n->data;
		if (streq(ban->mask, mask)) {
			if (!on) {
				if (m->stacker && m->stacker->put_listent)
					m->stacker->put_listent(m, 0, ban);
				mowgli_node_delete(&ban->n, list);
				free(ban);
			}
			return 1;
		}
	}

	if (on) {
		if (mowgli_list_size(list) >= MAXBANLIST) {
			/* should this be handed to stacker? */
			u_log(LG_VERBOSE, "+%c full, not adding %s",
		              m->info->ch, mask);
			m->errors |= MODE_ERR_LIST_FULL;
			return 1;
		}

		ban = malloc(sizeof(*ban));

		u_strlcpy(ban->mask, mask, 256);
		snf(FMT_USER, ban->setter, 256, "%I", m->setter);
		ban->time = NOW.tv_sec;

		if (m->stacker && m->stacker->put_listent)
			m->stacker->put_listent(m, 1, ban);
		mowgli_node_add(ban, &ban->n, list);
	}

	return 1;
}

int u_mode_process(u_modes *m, int parc, char **parv)
{
	int used, any = 0, on = 1;
	char *param;
	char *s = *parv++;
	char *unk = m->unk;

	*unk = '\0';
	m->errors = 0;
	parc--;

	if (strlen(s) > MAX_MODES)
		return -1;

	if (m->stacker && m->stacker->start)
		m->stacker->start(m);

	for (; *s; s++) {
		if (*s == '+' || *s == '-') {
			on = *s == '+';
			continue;
		}

		if (!(m->info = find_info(m->ctx->infotab, *s))) {
			m->errors |= MODE_ERR_UNK_CHAR;
			*unk++ = *s;
			continue;
		}

		if ((m->info->flags & MODE_OPER_ONLY) &&
		    !can_set_oper_only(m->setter)) {
			m->errors |= MODE_ERR_NOT_OPER;
			continue;
		}

		if ((m->info->flags & MODE_UNSET_ONLY) && on) {
			m->errors |= MODE_ERR_SET_UNSET_ONLY;
			continue;
		}

		any++;
		param = parc > 0 ? *parv : NULL;
		used = 0;

		switch (m->info->type) {
		case MODE_EXTERNAL:
			if (m->info->arg.fn) {
				used = m->info->arg.fn(m, on, param);
			} else {
				/* ??? */
				m->errors |= MODE_ERR_UNK_CHAR;
				*unk++ = *s;
			}
			break;

		case MODE_STATUS:
			used = do_mode_status(m, on, param);
			break;

		case MODE_FLAG:
			do_mode_flag(m, on);
			break;

		case MODE_LIST:
			used = do_mode_list(m, on, param);
			break;
		}

		if (used && parc) {
			parc--;
			parv++;
		}
	}

	*unk = '\0';

	if (m->stacker && m->stacker->end)
		m->stacker->end(m);

	if (m->ctx->sync)
		m->ctx->sync(m);

	return 0;
}

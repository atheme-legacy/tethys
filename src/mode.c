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

static int do_mode_banlist(u_modes *m, int on, char *param)
{
	/* TODO: implement */

	if (!param) {
		u_log(LG_INFO, "%I wants to view list %c", m->setter,
		      m->info->ch);
		return 0;
	}

	u_log(LG_INFO, "%I wants to %s in list %c", m->setter,
	      on ? "add" : "delete", param, on ? "to" : "from",
	      m->info->ch);

	return 0;
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

		case MODE_BANLIST:
			used = do_mode_banlist(m, on, param);
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

	return 0;
}

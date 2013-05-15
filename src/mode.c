/* ircd-micro, mode.c -- mode processing
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void changes_reset(c) u_mode_changes *c;
{
}

static void mode_put(c, on, ch, type, fmt, p)
u_mode_changes *c; char ch, *fmt; void *p;
{
	if (c->setting != on) {
		c->setting = on;
		*c->b++ = (on ? '+' : '-');
	}
	*c->b++ = ch;
	if (fmt != NULL)
		c->d += snf(type, c->data, c->data+512-c->d, fmt, p);
}

void u_mode_put(m, on, ch, fmt, p) u_modes *m; char ch, *fmt; void *p;
{
	u_mode_put_u(m, on, ch, fmt, p);
	u_mode_put_s(m, on, ch, fmt, p);
}

void u_mode_put_u(m, on, ch, fmt, p) u_modes *m; char ch, *fmt; void *p;
{
	mode_put(&m->u, on, ch, FMT_USER, fmt, p);
}

void u_mode_put_s(m, on, ch, fmt, p) u_modes *m; char ch, *fmt; void *p;
{
	mode_put(&m->s, on, ch, FMT_SERVER, fmt, p);
}

void u_mode_put_l(m, ch) u_modes *m; char ch;
{
	if (!strchr(m->list, ch)) {
		*m->l++ = ch;
		*m->l = '\0';
	}
}

static u_mode_info *find_info(info, ch) u_mode_info *info; char ch;
{
	for (; info->ch; info++) {
		if (info->ch == ch)
			return info;
	}
	return NULL;
}

void u_mode_process(m, infos, parc, parv)
u_modes *m; u_mode_info *infos; int parc; char **parv;
{
	int used, on = 1;
	char *s;
	u_mode_info *inf;

	changes_reset(&m->u);
	changes_reset(&m->s);
	m->l = m->list;
	*m->l = '\0';

	s = *parv++;
	parc--;

	for (; *s; s++) {
		if (*s == '+' || *s == '-') {
			on = *s == '+';
			continue;
		}

		/* TODO: *SAY SOMETHING* */
		if (!(inf = find_info(infos, *s)))
			continue;

		used = inf->cb(m, on, parc, parv);
		parc -= used;
		parv += used;
	}
}

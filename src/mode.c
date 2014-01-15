/* Tethys, mode.c -- mode processing
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static void changes_reset(u_mode_changes *c)
{
	c->setting = -1;
	*(c->b = c->buf) = '\0';
	*(c->d = c->data) = '\0';
}

static void changes_end(u_mode_changes *c)
{
	*c->b = '\0';
	*c->d = '\0';
}

static void mode_put(u_mode_changes *c, int on, char ch, int type,
                     char *fmt, void *p)
{
	if (c->setting != on) {
		c->setting = on;
		*c->b++ = (on ? '+' : '-');
	}
	*c->b++ = ch;
	if (fmt != NULL)
		c->d += snf(type, c->d, c->data+512-c->d, fmt, p);
}

void u_mode_put(u_modes *m, int on, char ch, char *fmt, void *p)
{
	u_mode_put_u(m, on, ch, fmt, p);
	u_mode_put_s(m, on, ch, fmt, p);
}

void u_mode_put_u(u_modes *m, int on, char ch, char *fmt, void *p)
{
	mode_put(&m->u, on, ch, FMT_USER, fmt, p);
}

void u_mode_put_s(u_modes *m, int on, char ch, char *fmt, void *p)
{
	mode_put(&m->s, on, ch, FMT_SERVER, fmt, p);
}

void u_mode_put_l(u_modes *m, char ch)
{
	if (!strchr(m->list, ch)) {
		*m->l++ = ch;
		*m->l = '\0';
	}
}

static u_mode_info *find_info(u_mode_info *info, char ch)
{
	for (; info->ch; info++) {
		if (info->ch == ch)
			return info;
	}
	return NULL;
}

void u_mode_process(u_modes *m, u_mode_info *infos, int parc, char **parv)
{
	int used, on = 1;
	char *s;

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
		if (!(m->info = find_info(infos, *s)))
			continue;

		used = m->info->cb(m, on, parc > 0 ? parv[0] : NULL);
		parc -= used;
		parv += used;
	}

	changes_end(&m->u);
	changes_end(&m->s);
}

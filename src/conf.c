/* ircd-micro, conf.c -- configuration parser
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

mowgli_patricia_t *u_conf_handlers = NULL;

void do_cb(char *key, char *val)
{
	u_conf_handler_t *cb;

	u_log(LG_FINE, "conf: %s=%s", key, val);

	cb = (u_conf_handler_t*)mowgli_patricia_retrieve(u_conf_handlers, key);

	if (!cb) {
		u_log(LG_WARN, "No config handler for %s=%s", key, val);
		return;
	}
	cb(key, val);
}

void skip_to_eol(FILE *f)
{
	while (!feof(f) && getc(f) != '\n');
}

void skip_ews(FILE *f) /* ews == effective whitespace */
{
	int c;
	for (;;) {
		c = getc(f);
		if (c == '#')
			skip_to_eol(f);
		else if (c == EOF || !isspace(c))
			break;
	}
	if (c != EOF)
		ungetc(c, f);
}

void read_quoted_value(FILE *f, char *p, int n)
{
	int c;

	while (!feof(f) && n > 1) {
		c = getc(f);
		if (c == '\\')
			c = getc(f);
		else if (c == '"')
			break;
		if (c == EOF)
			break;
		*p++ = c;
		n--;
	}

	*p = '\0';
}

void read_unquoted_value(FILE *f, char *p, int n)
{
	int c;

	while (!feof(f) && n > 1) {
		c = getc(f);
		if (c == EOF || isspace(c)) {
			if (c != EOF)
				ungetc(c, f);
			break;
		}
		*p++ = c;
		n--;
	}

	*p = '\0';
}

void read_value(FILE *f, char *p, int n)
{
	int c;

	skip_ews(f);

	c = getc(f);
	
	if (c == EOF) {
		return;
	} else if (c == '"') {
		read_quoted_value(f, p, n);
	} else {
		ungetc(c, f);
		read_unquoted_value(f, p, n);
	}
}

void conf_descend(char *key, char *value, FILE *f)
{
	int c, n = strlen(key);
	char *p = key + n;
	n = U_CONF_MAX_KEY - n;

	for (;;) {
		skip_ews(f);
		c = getc(f);
		if (c == '}' || c == EOF)
			return;
		ungetc(c, f);

		read_value(f, p, n);

		skip_ews(f);
		c = getc(f);
		if (c == EOF)
			return;

		if (c != '{') {
			ungetc(c, f);
			read_value(f, value, n);
			do_cb(key, value);

			skip_ews(f);
			c = getc(f);
		}

		if (c != '{') {
			ungetc(c, f);
			continue;
		}

		u_strlcat(key, ".", U_CONF_MAX_KEY);
		conf_descend(key, value, f);
	}
}

void u_conf_read(FILE *f)
{
	char key[U_CONF_MAX_KEY];
	char value[U_CONF_MAX_VALUE];

	key[0] = value[0] = '\0';

	skip_ews(f);
	conf_descend(key, value, f);
}

void u_conf_add_handler(char *key, u_conf_handler_t *cb)
{
	mowgli_patricia_add(u_conf_handlers, key, (void*)cb);
}

int init_conf(void)
{
	u_conf_handlers = mowgli_patricia_create(ascii_canonize);
	return u_conf_handlers ? 0 : -1;
}

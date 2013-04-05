/* ircd-micro, conf.c -- configuration parser
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

u_trie *u_conf_handlers = NULL;

void do_cb(key, val) char *key, *val;
{
	void (*cb)();

	cb = u_trie_get(u_conf_handlers, key);
	if (!cb) {
		u_log(LG_WARN, "No config handler for %s=%s", key, val);
		return;
	}
	cb(key, val);
}

void skip_to_eol(f) FILE *f;
{
	while (!feof(f) && getc(f) != '\n');
}

void skip_spaces(f) FILE *f;
{
	int c;
	for (;;) {
		c = getc(f);
		if (c == EOF || !isspace(c))
			break;
	}
	if (c != EOF)
		ungetc(c, f);
}

void read_quoted_value(f, p, n) FILE *f; char *p;
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

void read_unquoted_value(f, p, n) FILE *f; char *p;
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

void read_value(f, p, n) FILE *f; char *p;
{
	int c;
top:

	skip_spaces(f);

	c = getc(f);
	
	if (c == EOF) {
		return;
	} else if (c == '#') {
		skip_to_eol(f);
		goto top;
	} else if (c == '"') {
		read_quoted_value(f, p, n);
	} else {
		ungetc(c, f);
		read_unquoted_value(f, p, n);
	}
}

void conf_descend(key, value, f) char *key, *value; FILE *f;
{
	int c, n = strlen(key);
	char *p = key + n;
	n = U_CONF_MAX_KEY - n;

	for (;;) {
		skip_spaces(f);
		c = getc(f);
		if (c == '}' || c == EOF)
			return;
		ungetc(c, f);

		read_value(f, p, n);

		skip_spaces(f);
		c = getc(f);
		if (c == EOF) {
			return;
		} else if (c == '{') {
			u_strlcat(key, ".", U_CONF_MAX_KEY);
			conf_descend(key, value, f);
		} else {
			ungetc(c, f);
			read_value(f, value, n);
			do_cb(key, value);
		}
	}
}

void u_conf_read(f) FILE *f;
{
	char key[U_CONF_MAX_KEY];
	char value[U_CONF_MAX_VALUE];

	key[0] = value[0] = '\0';

	skip_spaces(f);
	conf_descend(key, value, f);
}

int init_conf()
{
	u_conf_handlers = u_trie_new(ascii_canonize);
	return u_conf_handlers ? 0 : -1;
}

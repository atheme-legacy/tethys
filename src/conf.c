#include "ircd.h"

void skip_to_eol(f)
FILE *f;
{
	int c;
	while (!feof(f) && getc(f) != '\n');
}

void skip_spaces(f)
FILE *f;
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

void read_quoted_value(f, p, n)
FILE *f;
char *p;
int n;
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

void read_unquoted_value(f, p, n)
FILE *f;
char *p;
int n;
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

void read_value(f, p, n)
FILE *f;
char *p;
int n;
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

void conf_descend(key, value, f, cb)
char *key, *value;
FILE *f;
void (*cb)();
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
			conf_descend(key, value, f, cb);
		} else {
			ungetc(c, f);
			read_value(f, value, n);
			cb(key, value);
		}
	}
}

void u_conf_read(f, cb)
FILE *f;
void (*cb)();
{
	char key[U_CONF_MAX_KEY];
	char value[U_CONF_MAX_VALUE];

	key[0] = value[0] = '\0';

	skip_spaces(f);
	conf_descend(key, value, f, cb);
}

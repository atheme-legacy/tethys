/* ircd-micro, upgrade.c -- self-exec
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#define UDB_LINE_SIZE 4096

#include "ircd.h"

struct u_udb {
	int reading;

	FILE *f;

	char line[UDB_LINE_SIZE];
	char buf[UDB_LINE_SIZE];
	char *p;
	ulong sz;
};

u_list *u_udb_save_hooks;
u_trie *u_udb_load_hooks;

void u_udb_row_start(u_udb *db, char *name)
{
	if (db->reading) {
		u_log(LG_ERROR, "Tried to start row while reading database!");
		return;
	}

	db->sz = snf(FMT_LOG, db->line, UDB_LINE_SIZE, "%s", name);
}

void u_udb_row_end(u_udb *db)
{
	fprintf(db->f, "%s\n", db->line);
}

void u_udb_put_s(u_udb *db, char *s)
{
	char *p = db->buf;

	while (*s) {
		switch (*s) {
		case ' ':
		case '\\':
			*p++ = '\\';
		default:
			*p++ = *s++;
		}
	}

	db->sz += snf(FMT_LOG, db->line + db->sz,
	              UDB_LINE_SIZE - db->sz, " %s", s);
}

void u_udb_put_i(u_udb *db, long n)
{
	char buf[512];

	snf(FMT_LOG, buf, 512, "%d", n);
	u_udb_put_s(db, buf);
}

char *u_udb_get_s(u_udb *db)
{
	char *s = db->buf;

	if (!db->reading) {
		u_log(LG_ERROR, "Tried to read word while writing database!");
		return NULL;
	}

	while (*db->p != ' ' && *db->p) {
		if (*db->p == '\\')
			db->p++;
		*s++ = *db->p++;
	}
	*s = '\0';

	return db->buf;
}

long u_udb_get_i(u_udb *db)
{
	char *s = u_udb_get_s(db);
	if (s != NULL)
		return atoi(s);
	return -1;
}

int init_upgrade(void)
{
	u_udb_save_hooks = malloc(sizeof(u_list));
	u_udb_load_hooks = u_trie_new(NULL);

	u_list_init(u_udb_save_hooks);

	return 0;
}

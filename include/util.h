/* Tethys, util.h -- various utilities
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_UTIL_H__
#define __INC_UTIL_H__

#define CIDR_ADDRSTRLEN (INET_ADDRSTRLEN+3)

typedef struct u_cidr u_cidr;

struct u_cidr {
	ulong addr;
	int netsize;
};

extern void u_cidr_to_str(u_cidr*, char*);
extern void u_str_to_cidr(char*, u_cidr*);
extern int u_cidr_match(u_cidr*, char*);

extern int matchmap(char *pat, char *string, char *map);
extern int match(char *pattern, char *string);
extern int matchirc(char*, char*); /* rfc1459 casemapping */
extern int matchcase(char*, char*); /* ascii casemapping */

extern int matchhash(char *hash, char *string);

#define streq(a,b) (!strcmp((a),(b)))

extern int mapcmp(char *s1, char *s2, char *map);
extern int casecmp(char *s1, char *s2);
extern int irccmp(char *s1, char *s2);

#define u_strlcpy mowgli_strlcpy
#define u_strlcat mowgli_strlcat
extern void u_ntop(struct in_addr*, char*);
extern void u_aton(char*, struct in_addr*);

extern char *cut(char **p, char *delim);

extern void null_canonize();
extern void rfc1459_canonize();
extern void ascii_canonize();

extern char *id_to_name(char *sid_or_uid);
extern char *name_to_id(char *nick_or_server);
extern char *ref_to_name(char *ref);
extern char *ref_to_id(char *ref);

extern bool exists(const char *path);

#include "conn.h"

extern char *ref_to_ref(u_conn *ctx, char *ref);
extern u_conn *ref_link(u_conn *ctx, char *ref);

extern char *conn_name(u_conn*);
extern char *conn_id(u_conn*);

extern int is_valid_nick(char *s);
extern int is_valid_ident(char *s);
extern int is_valid_sid(char *s);
extern int is_valid_chan(char *s);

extern int init_util();

static inline void mowgli_list_init(mowgli_list_t *list)
{
	memset(list, 0, sizeof(*list));
}

static inline mowgli_node_t *mowgli_list_add(mowgli_list_t *list, void *data)
{
	mowgli_node_t *n = mowgli_node_create();
	mowgli_node_add(data, n, list);
	return n;
}

static inline void *mowgli_list_delete(mowgli_node_t *n, mowgli_list_t *list)
{
	void *d = n->data;
	mowgli_node_delete(n, list);
	mowgli_node_free(n);
	return d;
}

static inline int mowgli_list_size(mowgli_list_t *list)
{
	return list->count;
}

static inline bool str_transform(char *ch, int (*filter)(int))
{
	char *c;
	bool pass = true;

	for (c = ch; *c; ++c)
	{
		char nc = filter(*c);
		if (nc == '\0')
			pass = false;
		else
			*c = nc;
	}

	return pass;
}

static inline void str_lower(char *ch)
{
	str_transform(ch, &tolower);
}

static inline void str_upper(char *ch)
{
	str_transform(ch, &toupper);
}

#endif

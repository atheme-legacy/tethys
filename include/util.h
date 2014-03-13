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

typedef unsigned long u_bitmask_set;
extern void u_bitmask_reset(u_bitmask_set*);
extern void u_bitmask_used(u_bitmask_set*, unsigned long);
extern unsigned long u_bitmask_alloc(u_bitmask_set*);
extern void u_bitmask_free(u_bitmask_set*, unsigned long);

extern unsigned long parse_size(char*);

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

extern void u_pton(const char*, struct sockaddr_storage *ss);

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
#include "link.h"

extern char *ref_to_ref(u_link *ctx, char *ref);
extern u_link *ref_link(u_link *ctx, char *ref);

extern char *link_name(u_link*);
extern char *link_id(u_link*);

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

static inline void json_oseto(mowgli_json_t *obj, const char *k, mowgli_json_t *v)
{
	mowgli_json_object_add(obj, k, v ? v : mowgli_json_null);
}

static inline void json_oseti(mowgli_json_t *obj, const char *k, int v)
{
	json_oseto(obj, k, mowgli_json_create_integer(v));
}

static inline void json_osetb(mowgli_json_t *obj, const char *k, bool v)
{
	json_oseto(obj, k, v ? mowgli_json_true : mowgli_json_false);
}

#define json_osetu   json_oseti /* XXX */
#define json_oseti64 json_oseti /* XXX */

static inline void json_osets(mowgli_json_t *obj, const char *k, const char *v)
{
	json_oseto(obj, k, v ? mowgli_json_create_string(v) : mowgli_json_null);
}

extern void json_osetb64(mowgli_json_t *obj, const char *k, const void *buf, size_t buf_len);
extern ssize_t json_ogetb64(mowgli_json_t *obj, const char *k, void *buf, size_t buf_len);

static inline void json_append(mowgli_json_t *arr, mowgli_json_t *v)
{
	mowgli_json_array_add(arr, v);
}

static inline mowgli_json_t *json_ogeto(mowgli_json_t *obj, const char *k)
{
	mowgli_json_t *v;
	if (MOWGLI_JSON_TAG(obj) != MOWGLI_JSON_TAG_OBJECT)
		return NULL;
	v = mowgli_json_object_retrieve(obj, k);
	if (!v || MOWGLI_JSON_TAG(v) == MOWGLI_JSON_TAG_NULL)
		return NULL;
	return v;
}

static inline mowgli_json_t *json_ogeto_c(mowgli_json_t *obj, const char *k)
{
	mowgli_json_t *v = json_ogeto(obj, k);
	if (MOWGLI_JSON_TAG(obj) != MOWGLI_JSON_TAG_OBJECT)
		return NULL;
	return v;
}

static inline mowgli_string_t *json_ogets(mowgli_json_t *obj, const char *k)
{
	mowgli_json_t *js;
	js = json_ogeto(obj, k);
	if (!js || MOWGLI_JSON_TAG(js) != MOWGLI_JSON_TAG_STRING)
		return NULL;
	return MOWGLI_JSON_STRING(js);
}

static inline mowgli_list_t *json_ogeta(mowgli_json_t *obj, const char *k)
{
	mowgli_json_t *js;
	js = json_ogeto(obj, k);
	if (!js || MOWGLI_JSON_TAG(js) != MOWGLI_JSON_TAG_ARRAY)
		return NULL;
	return MOWGLI_JSON_ARRAY(js);
}

static inline bool json_ogeti(mowgli_json_t *obj, const char *k, int *v)
{
	mowgli_json_t *ji;
	ji = json_ogeto(obj, k);
	if (!ji || MOWGLI_JSON_TAG(ji) != MOWGLI_JSON_TAG_INTEGER)
		return false;
	*v = MOWGLI_JSON_INTEGER(ji);
	return true;
}

static inline bool json_ogeti64(mowgli_json_t *obj, const char *k, int64_t *v)
{
	int ok;
	int vv = 0;
	/* XXX */
	ok = json_ogeti(obj, k, &vv);
	if (ok)
		*v = vv;
	return ok;
}

static inline bool json_ogetu64(mowgli_json_t *obj, const char *k, uint64_t *v)
{
	return json_ogeti64(obj, k, (int64_t*)v);
}

static inline bool json_ogetu(mowgli_json_t *obj, const char *k, unsigned *v)
{
	/* XXX */
	return json_ogeti(obj, k, (int*)v);
}

static inline bool json_ogetb(mowgli_json_t *obj, const char *k)
{
	mowgli_json_t *jb = json_ogeto(obj, k);

	/* strict equality */
	if (MOWGLI_JSON_TAG(jb) != MOWGLI_JSON_TAG_BOOLEAN)
		return false;

	return MOWGLI_JSON_BOOLEAN(jb);
}

extern int set_cloexec(int fd);

inline static size_t base64_inflate_size(size_t len) {
	return 4*((len+2)/3);
}

inline static size_t base64_deflate_size(size_t len) {
	return ((len/4)*3)+3;
}

extern size_t base64_encode(const void *in_buf, size_t in_buf_len, char *out_buf, char *out_cur);
extern size_t base64_decode(const char *in_buf, size_t in_buf_len, void *out_buf);

/* vim: set noet: */
#endif

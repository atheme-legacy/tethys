/* ircd-micro, auth.c -- authentication management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static char *msg_noauthblocks = "No auth{} blocks; using default auth settings";
static char *msg_classnotfound = "Auth block %s asks for class %s, but no such class exists! Using default class";
static char *msg_authnotfound = "Oper block %s asks for auth %s, but no such auth exists! Ignoring auth setting";
static char *msg_timeouttooshort = "Timeout of %d seconds for class %s too short. Setting to %d seconds";

static u_class class_default =
	{ "default", 300 };
static u_auth auth_default =
	{ "default", "default", &class_default, { 0, 0 }, "" };

static u_map *all_classes;
static u_map *all_auths;
static u_map *all_opers;

static u_list auth_list;

u_auth *u_find_auth(conn) u_conn *conn;
{
	u_list *n;
	u_auth *auth;

	if (u_list_size(&all_auths) == 0) {
		u_log(LG_WARN, msg_noauthblocks);
		return &auth_default;
	}

	U_LIST_EACH(n, &auth_list) {
		auth = n->data;
		if (!u_cidr_match(&auth->cidr, conn->ip))
			continue;
		if (auth->pass[0]) {
			if (!conn->pass || strcmp(auth->pass, conn->pass))
				continue;
		}

		if (auth->cls == NULL) {
			auth->cls = u_map_get(all_classes, auth->classname);
			if (auth->cls == NULL) {
				u_log(LG_WARN, msg_classnotfound,
				      auth->name, auth->classname);
				auth->cls = &class_default;
			}
		}

		return auth;
	}

	return NULL;
}

u_oper *u_find_oper(auth, name, pass) u_auth *auth; char *name, *pass;
{
	u_oper *oper;

	oper = u_map_get(all_opers, name);
	if (oper == NULL)
		return NULL;

	if (oper->auth == NULL && oper->authname[0]) {
		oper->auth = u_map_get(all_auths, oper->authname);
		if (auth->cls == NULL) {
			u_log(LG_WARN, msg_authnotfound,
			      oper->name, oper->authname);
		}
	}

	if (oper->auth != NULL && oper->auth != auth)
		return NULL;

	if (strcmp(oper->pass, pass))
		return NULL;

	return oper;
}

#define CONF_CHECK(var, key, need) do { \
	if ((var) == NULL) { \
		u_log(LG_ERROR, "auth conf: %s before %s!", key, need); \
		return; \
	} } while(0)

static u_class *cur_class = NULL;

void conf_class(key, val) char *key, *val;
{
	cur_class = malloc(sizeof(*cur_class));
	u_strlcpy(cur_class->name, val, MAXCLASSNAME+1);
	cur_class->timeout = 300;
}

void conf_class_timeout(key, val) char *key, *val;
{
	CONF_CHECK(cur_class, key, "class");
	cur_class->timeout = atoi(val);
	if (cur_class->timeout < 15) {
		u_log(LG_WARN, msg_timeouttooshort, cur_class->timeout,
		      cur_class->name, 15);
		cur_class->timeout = 15;
	}
}

static u_auth *cur_auth = NULL;

void conf_auth(key, val) char *key, *val;
{
	cur_auth = malloc(sizeof(*cur_auth));

	memset(cur_auth, 0, sizeof(*cur_auth));
	u_strlcpy(cur_auth->name, val, MAXAUTHNAME+1);

	u_map_set(all_auths, val, cur_auth);
	cur_auth->n = u_list_add(&auth_list, cur_auth);
}

void conf_auth_class(key, val) char *key, *val;
{
	CONF_CHECK(cur_auth, key, "auth");
	u_strlcpy(cur_auth->classname, val, MAXCLASSNAME+1);
}

void conf_auth_cidr(key, val) char *key, *val;
{
	CONF_CHECK(cur_auth, key, "auth");
	u_str_to_cidr(val, &cur_auth->cidr);
}

void conf_auth_password(key, val) char *key, *val;
{
	CONF_CHECK(cur_auth, key, "auth");
	u_strlcpy(cur_auth->pass, val, MAXPASSWORD+1);
}

static u_oper *cur_oper = NULL;

void conf_oper(key, val) char *key, *val;
{
	cur_oper = malloc(sizeof(*cur_oper));

	u_strlcpy(cur_oper->name, val, MAXOPERNAME+1);
	cur_oper->pass[0] = '\0';
	cur_oper->authname[0] = '\0';
	cur_oper->auth = NULL;

	u_map_set(all_opers, val, cur_oper);
}

void conf_oper_password(key, val) char *key, *val;
{
	CONF_CHECK(cur_oper, key, "oper");
	u_strlcpy(cur_oper->pass, val, MAXPASSWORD+1);
}

void conf_oper_auth(key, val) char *key, *val;
{
	CONF_CHECK(cur_oper, key, "oper");
	u_strlcpy(cur_oper->authname, val, MAXAUTHNAME+1);
}

int init_auth()
{
	all_classes = u_map_new(1);
	all_auths = u_map_new(1);
	all_opers = u_map_new(1);

	if (!all_classes || !all_auths || !all_opers)
		return -1;

	u_list_init(&auth_list);

	u_trie_set(u_conf_handlers, "class", conf_class);
	u_trie_set(u_conf_handlers, "class.timeout", conf_class_timeout);

	u_trie_set(u_conf_handlers, "auth", conf_auth);
	u_trie_set(u_conf_handlers, "auth.class", conf_auth_class);
	u_trie_set(u_conf_handlers, "auth.cidr", conf_auth_cidr);
	u_trie_set(u_conf_handlers, "auth.password", conf_auth_password);

	u_trie_set(u_conf_handlers, "oper", conf_oper);
	u_trie_set(u_conf_handlers, "oper.password", conf_oper_password);
	u_trie_set(u_conf_handlers, "oper.auth", conf_oper_auth);

	return 0;
}

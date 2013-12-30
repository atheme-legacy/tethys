/* ircd-micro, auth.c -- authentication management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static char *msg_noauthblocks = "No auth{} blocks; using default auth settings";
static char *msg_nolinkblocks = "Attempted to find link{} block, but no such blocks! Unexpected server connect attempt?";
static char *msg_classnotfound = "%s block %s asks for class %s, but no such class exists! Using default class";
static char *msg_authnotfound = "Oper block %s asks for auth %s, but no such auth exists! Ignoring auth setting";
static char *msg_timeouttooshort = "Timeout of %d seconds for class %s too short. Setting to %d seconds";

static u_class class_default =
	{ "default", 300 };
static u_auth auth_default =
	{ "default", "default", &class_default, { 0, 0 }, "" };

u_map *all_classes;
u_map *all_auths;
u_map *all_opers;
u_map *all_links;

static u_list auth_list;
static u_list link_list;

u_auth *u_find_auth(u_conn *conn)
{
	u_list *n;
	u_auth *auth;

	if (u_list_size(&auth_list) == 0) {
		u_log(LG_WARN, msg_noauthblocks);
		return &auth_default;
	}

	U_LIST_EACH(n, &auth_list) {
		auth = n->data;
		if (!u_cidr_match(&auth->cidr, conn->ip))
			continue;
		if (auth->pass[0]) {
			if (!conn->pass || !matchhash(auth->pass, conn->pass))
				continue;
		}

		if (auth->cls == NULL) {
			auth->cls = u_map_get(all_classes, auth->classname);
			if (auth->cls == NULL) {
				u_log(LG_WARN, msg_classnotfound,
				      "Auth", auth->name, auth->classname);
				auth->cls = &class_default;
			}
		}

		return auth;
	}

	return NULL;
}

u_oper *u_find_oper(u_auth *auth, char *name, char *pass)
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

	if (!matchhash(oper->pass, pass))
		return NULL;

	return oper;
}

u_link *u_find_link(u_conn *conn)
{
	u_list *n;
	u_link *link;

	if (u_list_size(&link_list) == 0) {
		u_log(LG_WARN, msg_nolinkblocks);
		return NULL;
	}

	U_LIST_EACH(n, &link_list) {
		link = n->data;

		if (!streq(link->host, conn->ip))
			continue;
		if (link->recvpass[0]) {
			if (!conn->pass || !streq(link->recvpass, conn->pass))
				continue;
		}

		if (link->cls == NULL) {
			link->cls = u_map_get(all_classes, link->classname);
			if (link->cls == NULL) {
				u_log(LG_WARN, msg_classnotfound,
				      "Link", link->name, link->classname);
				link->cls = &class_default;
			}
		}

		return link;
	}

	return NULL;
}

#define CONF_CHECK(var, key, need) do { \
	if ((var) == NULL) { \
		u_log(LG_ERROR, "auth conf: %s before %s!", key, need); \
		return; \
	} } while(0)

static u_class *cur_class = NULL;

void conf_class(char *key, char *val)
{
	cur_class = malloc(sizeof(*cur_class));

	u_strlcpy(cur_class->name, val, MAXCLASSNAME+1);
	cur_class->timeout = 300;

	u_map_set(all_classes, val, cur_class);
}

void conf_class_timeout(char *key, char *val)
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

void conf_auth(char *key, char *val)
{
	cur_auth = malloc(sizeof(*cur_auth));

	memset(cur_auth, 0, sizeof(*cur_auth));
	u_strlcpy(cur_auth->name, val, MAXAUTHNAME+1);

	u_map_set(all_auths, val, cur_auth);
	cur_auth->n = u_list_add(&auth_list, cur_auth);
}

void conf_auth_class(char *key, char *val)
{
	CONF_CHECK(cur_auth, key, "auth");
	u_strlcpy(cur_auth->classname, val, MAXCLASSNAME+1);
}

void conf_auth_cidr(char *key, char *val)
{
	CONF_CHECK(cur_auth, key, "auth");
	u_str_to_cidr(val, &cur_auth->cidr);
}

void conf_auth_password(char *key, char *val)
{
	CONF_CHECK(cur_auth, key, "auth");
	u_strlcpy(cur_auth->pass, val, MAXPASSWORD+1);
}

static u_oper *cur_oper = NULL;

void conf_oper(char *key, char *val)
{
	cur_oper = malloc(sizeof(*cur_oper));

	u_strlcpy(cur_oper->name, val, MAXOPERNAME+1);
	cur_oper->pass[0] = '\0';
	cur_oper->authname[0] = '\0';
	cur_oper->auth = NULL;

	u_map_set(all_opers, val, cur_oper);
}

void conf_oper_password(char *key, char *val)
{
	CONF_CHECK(cur_oper, key, "oper");
	u_strlcpy(cur_oper->pass, val, MAXPASSWORD+1);
}

void conf_oper_auth(char *key, char *val)
{
	CONF_CHECK(cur_oper, key, "oper");
	u_strlcpy(cur_oper->authname, val, MAXAUTHNAME+1);
}

static u_link *cur_link = NULL;

void conf_link(char *key, char *val)
{
	cur_link = malloc(sizeof(*cur_link));

	memset(cur_link, 0, sizeof(*cur_link));
	u_strlcpy(cur_link->name, val, MAXSERVERNAME+1);

	u_map_set(all_links, val, cur_link);
	cur_link->n = u_list_add(&link_list, cur_link);
}

void conf_link_host(char *key, char *val)
{
	CONF_CHECK(cur_link, key, "link");
	u_strlcpy(cur_link->host, val, INET_ADDRSTRLEN);
}

void conf_link_sendpass(char *key, char *val)
{
	CONF_CHECK(cur_link, key, "link");
	u_strlcpy(cur_link->sendpass, val, MAXPASSWORD+1);
}

void conf_link_recvpass(char *key, char *val)
{
	CONF_CHECK(cur_link, key, "link");
	u_strlcpy(cur_link->recvpass, val, MAXPASSWORD+1);
}

void conf_link_class(char *key, char *val)
{
	CONF_CHECK(cur_link, key, "link");
	u_strlcpy(cur_link->classname, val, MAXCLASSNAME+1);
}

int init_auth(void)
{
	all_classes = u_map_new(1);
	all_auths = u_map_new(1);
	all_opers = u_map_new(1);
	all_links = u_map_new(1);

	if (!all_classes || !all_auths || !all_opers || !all_links)
		return -1;

	u_list_init(&auth_list);
	u_list_init(&link_list);

	u_trie_set(u_conf_handlers, "class", conf_class);
	u_trie_set(u_conf_handlers, "class.timeout", conf_class_timeout);

	u_trie_set(u_conf_handlers, "auth", conf_auth);
	u_trie_set(u_conf_handlers, "auth.class", conf_auth_class);
	u_trie_set(u_conf_handlers, "auth.cidr", conf_auth_cidr);
	u_trie_set(u_conf_handlers, "auth.password", conf_auth_password);

	u_trie_set(u_conf_handlers, "oper", conf_oper);
	u_trie_set(u_conf_handlers, "oper.password", conf_oper_password);
	u_trie_set(u_conf_handlers, "oper.auth", conf_oper_auth);

	u_trie_set(u_conf_handlers, "link", conf_link);
	u_trie_set(u_conf_handlers, "link.host", conf_link_host);
	u_trie_set(u_conf_handlers, "link.sendpass", conf_link_sendpass);
	u_trie_set(u_conf_handlers, "link.recvpass", conf_link_recvpass);
	u_trie_set(u_conf_handlers, "link.class", conf_link_class);

	return 0;
}

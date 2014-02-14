/* Tethys, auth.c -- authentication management
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
static u_auth_block auth_default =
	{ "default", "default", &class_default, { 0, 0 }, "" };

u_map *all_classes;
u_map *all_auths;
u_map *all_opers;
u_map *all_links;

static mowgli_list_t auth_list;
static mowgli_list_t link_list;

static mowgli_patricia_t *u_conf_auth_handlers = NULL;
static mowgli_patricia_t *u_conf_class_handlers = NULL;
static mowgli_patricia_t *u_conf_oper_handlers = NULL;
static mowgli_patricia_t *u_conf_link_handlers = NULL;

u_auth_block *u_find_auth(u_conn *conn)
{
	mowgli_node_t *n;
	u_auth_block *auth;

	if (mowgli_list_size(&auth_list) == 0) {
		u_log(LG_WARN, msg_noauthblocks);
		return &auth_default;
	}

	MOWGLI_LIST_FOREACH(n, auth_list.head) {
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

u_oper_block *u_find_oper(u_auth_block *auth, char *name, char *pass)
{
	u_oper_block *oper;

	oper = u_map_get(all_opers, name);
	if (oper == NULL)
		return NULL;

	if (oper->auth == NULL && oper->authname[0]) {
		oper->auth = u_map_get(all_auths, oper->authname);
		if (auth && auth->cls == NULL) {
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

u_link_block *u_find_link(u_server *sv)
{
	if (mowgli_list_size(&link_list) == 0) {
		u_log(LG_WARN, msg_nolinkblocks);
		return NULL;
	}

	return u_map_get(all_links, sv->name);
}

static u_class *cur_class = NULL;

void conf_class(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	cur_class = malloc(sizeof(*cur_class));

	u_strlcpy(cur_class->name, ce->vardata, MAXCLASSNAME+1);
	cur_class->timeout = 300;

	u_map_set(all_classes, cur_class->name, cur_class);

	u_conf_traverse(cf, ce->entries, u_conf_class_handlers);
}

void conf_class_timeout(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	cur_class->timeout = atoi(ce->vardata);
	if (cur_class->timeout < 15) {
		u_log(LG_WARN, msg_timeouttooshort, cur_class->timeout,
		      cur_class->name, 15);
		cur_class->timeout = 15;
	}
}

static u_auth_block *cur_auth = NULL;

void conf_auth(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	cur_auth = malloc(sizeof(*cur_auth));

	memset(cur_auth, 0, sizeof(*cur_auth));
	u_strlcpy(cur_auth->name, ce->vardata, MAXAUTHNAME+1);

	u_map_set(all_auths, cur_auth->name, cur_auth);
	mowgli_node_add(cur_auth, &cur_auth->n, &auth_list);

	u_conf_traverse(cf, ce->entries, u_conf_auth_handlers);
}

void conf_auth_class(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_strlcpy(cur_auth->classname, ce->vardata, MAXCLASSNAME+1);
}

void conf_auth_cidr(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_str_to_cidr(ce->vardata, &cur_auth->cidr);
}

void conf_auth_password(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_strlcpy(cur_auth->pass, ce->vardata, MAXPASSWORD+1);
}

static u_oper_block *cur_oper = NULL;

void conf_oper(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	cur_oper = malloc(sizeof(*cur_oper));

	u_strlcpy(cur_oper->name, ce->vardata, MAXOPERNAME+1);
	cur_oper->pass[0] = '\0';
	cur_oper->authname[0] = '\0';
	cur_oper->auth = NULL;

	u_map_set(all_opers, cur_oper->name, cur_oper);

	u_conf_traverse(cf, ce->entries, u_conf_oper_handlers);
}

void conf_oper_password(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_strlcpy(cur_oper->pass, ce->vardata, MAXPASSWORD+1);
}

void conf_oper_auth(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_strlcpy(cur_oper->authname, ce->vardata, MAXAUTHNAME+1);
}

static u_link_block *cur_link = NULL;

void conf_link(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	cur_link = malloc(sizeof(*cur_link));

	memset(cur_link, 0, sizeof(*cur_link));
	u_strlcpy(cur_link->name, ce->vardata, MAXSERVERNAME+1);

	u_map_set(all_links, cur_link->name, cur_link);
	mowgli_node_add(cur_link, &cur_link->n, &link_list);

	u_conf_traverse(cf, ce->entries, u_conf_link_handlers);
}

void conf_link_host(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_strlcpy(cur_link->host, ce->vardata, INET_ADDRSTRLEN);
}

void conf_link_sendpass(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_strlcpy(cur_link->sendpass, ce->vardata, MAXPASSWORD+1);
}

void conf_link_recvpass(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_strlcpy(cur_link->recvpass, ce->vardata, MAXPASSWORD+1);
}

void conf_link_class(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_strlcpy(cur_link->classname, ce->vardata, MAXCLASSNAME+1);
}

int init_auth(void)
{
	all_classes = u_map_new(1);
	all_auths = u_map_new(1);
	all_opers = u_map_new(1);
	all_links = u_map_new(1);

	if (!all_classes || !all_auths || !all_opers || !all_links)
		return -1;

	mowgli_list_init(&auth_list);
	mowgli_list_init(&link_list);

	u_conf_class_handlers = mowgli_patricia_create(ascii_canonize);

	u_conf_add_handler("class", conf_class, NULL);
	u_conf_add_handler("timeout", conf_class_timeout, u_conf_class_handlers);

	u_conf_auth_handlers = mowgli_patricia_create(ascii_canonize);

	u_conf_add_handler("auth", conf_auth, NULL);
	u_conf_add_handler("class", conf_auth_class, u_conf_auth_handlers);
	u_conf_add_handler("cidr", conf_auth_cidr, u_conf_auth_handlers);
	u_conf_add_handler("password", conf_auth_password, u_conf_auth_handlers);

	u_conf_oper_handlers = mowgli_patricia_create(ascii_canonize);

	u_conf_add_handler("oper", conf_oper, NULL);
	u_conf_add_handler("password", conf_oper_password, u_conf_oper_handlers);
	u_conf_add_handler("auth", conf_oper_auth, u_conf_oper_handlers);

	u_conf_link_handlers = mowgli_patricia_create(ascii_canonize);

	u_conf_add_handler("link", conf_link, NULL);
	u_conf_add_handler("host", conf_link_host, u_conf_link_handlers);
	u_conf_add_handler("sendpass", conf_link_sendpass, u_conf_link_handlers);
	u_conf_add_handler("recvpass", conf_link_recvpass, u_conf_link_handlers);
	u_conf_add_handler("class", conf_link_class, u_conf_link_handlers);

	return 0;
}

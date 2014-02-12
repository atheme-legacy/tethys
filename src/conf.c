/* Tethys, conf.c -- configuration parser
   Copyright (C) 2013 Alex Iadicicco
   Copyright (C) 2014 William Pitcock

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

struct conf_handler {
	const char *key;
	u_conf_handler_t *cb;
	u_module *owner;
};

static mowgli_patricia_t *u_conf_handlers = NULL;

void u_conf_traverse(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce,
                     mowgli_patricia_t *handlers)
{
	struct conf_handler *h;
	mowgli_config_file_entry_t *cce;

	if (!handlers)
		return;

	MOWGLI_ITER_FOREACH(cce, ce) {
		u_log(LG_FINE, "conf: %s=%s", cce->varname, cce->vardata);

		h = mowgli_patricia_retrieve(handlers, cce->varname);

		if (h == NULL) {
			u_log(LG_WARN, "No config handler for %s=%s",
			      cce->varname, cce->vardata);
			continue;
		}

		h->cb(cf, cce);
	}
}

bool u_conf_read(const char *path)
{
	mowgli_config_file_t *cf;

	cf = mowgli_config_file_load(path);
	if (cf == NULL)
		return false;

	u_hook_call(u_hook_get(HOOK_CONF_START), cf);

	u_conf_traverse(cf, cf->entries, u_conf_handlers);

	u_hook_call(u_hook_get(HOOK_CONF_END), cf);

	mowgli_config_file_free(cf);

	return true;
}

void u_conf_add_handler(const char *key, u_conf_handler_t *cb, mowgli_patricia_t *handlers)
{
	struct conf_handler *h = malloc(sizeof(*h));

	if (!u_conf_handlers) {
		u_conf_handlers = mowgli_patricia_create(ascii_canonize);
		if (!u_conf_handlers)
			abort();
	}

	if (!handlers)
		handlers = u_conf_handlers;

	h->key = key;
	h->cb = cb;
	h->owner = u_module_loading();

	mowgli_patricia_add(handlers, key, h);
}

static void *on_module_unload(void *unused, void *m)
{
	mowgli_patricia_iteration_state_t state;
	struct conf_handler *h;

	if (!u_conf_handlers)
		return NULL;

	MOWGLI_PATRICIA_FOREACH(h, &state, u_conf_handlers) {
		if (h->owner != m)
			continue;

		mowgli_patricia_delete(u_conf_handlers, h->key);
		free(h);
	}

	return NULL;
}

int init_conf(void)
{
	u_hook_add(HOOK_MODULE_UNLOAD, on_module_unload, NULL);

	return 0;
}

/* Tethys, module.c -- modules
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static mowgli_list_t module_load_stack;

u_module *u_module_loading(void)
{
	if (module_load_stack.count == 0)
		return NULL;

	return module_load_stack.head->data;
}

static void module_load_push(u_module *m)
{
	mowgli_node_t *n = mowgli_node_create();
	mowgli_node_add_head(m, n, &module_load_stack);
}

static void module_load_pop(u_module *m)
{
	mowgli_node_t *n;

	if (module_load_stack.count == 0)
		return;

	n = module_load_stack.head;

	if (!m || n->data != m)
		return;

	mowgli_node_delete(n, &module_load_stack);
	mowgli_node_free(n);
}

static const char *module_find(const char *name)
{
	static char buf[2048];

	/* XXX: hardcoded path separators and file extensions */

	snprintf(buf, 2048, "modules/%s.so", name);

	if (exists(buf))
		return buf;

	return NULL;
}

static const char *module_load(u_module **mp, const char *name)
{
	const char *path, *error;
	u_module *m;

	*mp = NULL;

	if (!(path = module_find(name)))
		return "Could not find the specified module";

	m = malloc(sizeof(*m));
	m->module = NULL;

	error = "Could not open the specified module";
	if (!(m->module = mowgli_module_open(path)))
		goto fail;

	error = "Module missing __module_info. Probably not a module";
	if (!(m->info = mowgli_module_symbol(m->module, "__module_info")))
		goto fail;

	error = "Module name differs from declared module name";
	if (strcmp(m->info->name, name))
		goto fail;

	m->flags = 0;

	error = "Module major ABI revision mismatch. Cannot load!";
	if (m->info->abi_major != MODULE_ABI_MAJOR)
		goto fail;

	if (m->info->abi_minor != MODULE_ABI_MINOR) {
		u_log(LG_WARN, "Module %s has minor ABI revision mismatch",
		      m->info->name);
	}

	module_load_push(m);
	if (m->info->init) {
		error = "Module initalization failed";
		if (m->info->init(m) < 0)
			goto fail;
	}
	module_load_pop(m);

	m->flags |= MODULE_LOADED;

	u_hook_call(HOOK_MODULE_LOAD, m);

	*mp = m;
	return NULL;

fail:
	module_load_pop(m);
	if (m->module)
		mowgli_module_close(m->module);
	free(m);
	return error;
}

static void module_unload(u_module *m)
{
	u_hook_call(HOOK_MODULE_UNLOAD, m);

	if (m->info->deinit)
		m->info->deinit(m);

	mowgli_module_close(m->module);
	free(m);
}

mowgli_patricia_t *u_modules = NULL;

static u_module *do_module_load(const char *name, bool unique)
{
	u_module *m;
	const char *error;

	if ((m = u_module_find(name)) != NULL) {
		if (!unique)
			return m;
		u_log(LG_ERROR, "Module %s already loaded", name);
		return NULL;
	}

	u_log(LG_INFO, "Loading module %s", name);

	if ((error = module_load(&m, name)) != NULL) {
		u_log(LG_ERROR, "Failed to load module %s: %s", name, error);
		return NULL;
	}

	mowgli_patricia_add(u_modules, m->info->name, m);

	return m;
}

u_module *u_module_load(const char *name)
{
	return do_module_load(name, true);
}

u_module *u_module_find(const char *name)
{
	return mowgli_patricia_retrieve(u_modules, name);
}

u_module *u_module_find_or_load(const char *name)
{
	return do_module_load(name, false);
}

bool u_module_unload(const char *name)
{
	u_module *m;

	if (!(m = u_module_find(name)))
		return false;

	if (m->flags & MODULE_PERMANENT) {
		u_log(LG_ERROR, "Cannot unload permanent module %s", name);
		return false;
	}

	u_log(LG_INFO, "Unloading module %s", name);

	mowgli_patricia_delete(u_modules, m->info->name);
	module_unload(m);

	return true;
}

static u_module *do_module_reload(const char *name, bool need_unload)
{
	if (!u_module_unload(name) && need_unload) {
		u_log(LG_ERROR, "Module %s not loaded", name);
		return NULL;
	}

	return u_module_load(name);
}

u_module *u_module_reload(const char *name)
{
	return do_module_reload(name, true);
}

u_module *u_module_reload_or_load(const char *name)
{
	return do_module_reload(name, false);
}

static void conf_loadmodule(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce)
{
	u_module_load(ce->vardata);
}

int init_module(void)
{
	/* no canonization, because module names correspond to filenames, so
	   core/message and core/Message could very well be different modules
	   (but you won't see that sort of nonsense in the official repo). */
	if (!(u_modules = mowgli_patricia_create(NULL)))
		return -1;

	u_conf_add_handler("loadmodule", conf_loadmodule, NULL);

	return 0;
}

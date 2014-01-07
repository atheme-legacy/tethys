/* ircd-micro, hook.c -- hook and aspect system
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static mowgli_patricia_t *all_hooks;

static u_hook *add_hook(const char *name)
{
	u_hook *h;

	if ((h = mowgli_patricia_retrieve(all_hooks, name)) != NULL)
		return h;

	h = malloc(sizeof(*h));
	h->name = name; /* is this ok? */
	mowgli_list_init(&h->callbacks);

	mowgli_patricia_add(all_hooks, h->name, h);

	return h;
}

static void delete_hook(u_hook *h)
{
	if (h->callbacks.count != 0) {
		u_log(LG_ERROR, "Refusing to delete hook with remaining "
		                "callbacks (because lazy)");
		return;
	}

	mowgli_patricia_delete(all_hooks, h->name);
	free(h);
}

/* does not check for duplicates! */
void u_hook_add(const char *name, u_hook_fn_t *fn, void *priv)
{
	u_hook *hook;
	u_hook_cb *cb;

	if (!fn) {
		u_log(LG_ERROR, "Tried to hook a null callback to %s!", name);
		return;
	}

	u_log(LG_DEBUG, "HOOK: %p(%p) to %s", fn, priv, name);

	hook = add_hook(name);

	cb = malloc(sizeof(*cb));
	cb->fn = fn;
	cb->priv = priv;
	cb->owner = u_module_loading();

	mowgli_node_add(cb, &cb->n, &hook->callbacks);
}

void u_hook_delete(const char *name, u_hook_fn_t *fn, void *priv)
{
	u_hook *hook;
	mowgli_node_t *n, *tn;

	if ((hook = mowgli_patricia_retrieve(all_hooks, name)) == NULL)
		return;

	MOWGLI_LIST_FOREACH_SAFE(n, tn, hook->callbacks.head) {
		u_hook_cb *cb = n->data;
		if (cb->fn != fn || cb->priv != priv)
			continue;
		mowgli_node_delete(&cb->n, &hook->callbacks);
		free(cb);
	}

	if (hook->callbacks.count == 0)
		delete_hook(hook);
}

/* This method simply calls all the callbacks associated with the particular
   hook. The return value is ignored */
void u_hook_call(const char *name, void *arg)
{
	u_hook *hook;
	mowgli_node_t *n, *tn;

	u_log(LG_DEBUG, "HOOK:CALL: %s", name);

	if ((hook = mowgli_patricia_retrieve(all_hooks, name)) == NULL)
		return;

	MOWGLI_LIST_FOREACH_SAFE(n, tn, hook->callbacks.head) {
		u_hook_cb *cb = n->data;
		u_log(LG_DEBUG, "           %p(%p, %p)", cb->fn, cb->priv, arg);
		cb->fn(cb->priv, arg);
	}
}

/* This method calls each hook in the order it was added, finding the first
   one to return non-NULL. This value is returned */
void *u_hook_first(const char *name, void *arg)
{
	u_hook *hook;
	mowgli_node_t *n, *tn;
	void *p;

	u_log(LG_DEBUG, "HOOK:FIRST: %s", name);

	if ((hook = mowgli_patricia_retrieve(all_hooks, name)) == NULL)
		return NULL;

	MOWGLI_LIST_FOREACH_SAFE(n, tn, hook->callbacks.head) {
		u_hook_cb *cb = n->data;
		u_log(LG_DEBUG, "            %p(%p, %p)", cb->fn, cb->priv, arg);
		if ((p = cb->fn(cb->priv, arg)) != NULL)
			return p;
	}

	return NULL;
}

/* This method calls each hook in the order it was added. If the return
   value is non-NULL, it is added to the list. This list is returned. The
   caller should pass the list to u_hook_all_cleanup when finished. */
mowgli_list_t *u_hook_all(const char *name, void *arg)
{
	u_hook *hook;
	mowgli_node_t *n, *tn;
	mowgli_list_t *list;
	void *p;

	u_log(LG_DEBUG, "HOOK:ALL: %s", name);

	list = mowgli_list_create();

	if ((hook = mowgli_patricia_retrieve(all_hooks, name)) == NULL)
		return list;

	MOWGLI_LIST_FOREACH_SAFE(n, tn, hook->callbacks.head) {
		u_hook_cb *cb = n->data;
		u_log(LG_DEBUG, "          %p(%p, %p)", cb->fn, cb->priv, arg);
		if ((p = cb->fn(cb->priv, arg)) != NULL)
			mowgli_node_add(p, mowgli_node_create(), list);
	}

	return list;
}

void u_hook_all_cleanup(mowgli_list_t *list)
{
	mowgli_node_t *n, *tn;

	MOWGLI_LIST_FOREACH_SAFE(n, tn, list->head) {
		mowgli_node_delete(n, list);
		mowgli_node_free(n);
	}

	mowgli_list_free(list);
}

int init_hook(void)
{
	all_hooks = mowgli_patricia_create(NULL);

	return all_hooks == NULL ? -1 : 0;
}

/* ircd-micro, hook.h -- hook and aspect system
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_HOOK_H__
#define __INC_HOOK_H__

/* some tokens, to make using void* as a return type less clunky */
#define U_HOOK_FALSE NULL
#define U_HOOK_TRUE ((void*)1)

typedef void *(u_hook_fn_t)(void *priv, void *arg);

typedef struct u_hook u_hook;
typedef struct u_hook_cb u_hook_cb;

struct u_hook {
	const char *name;
	mowgli_list_t callbacks;
};

struct u_hook_cb {
	u_hook_fn_t *fn;
	void *priv;

	u_module *owner;
	mowgli_node_t n;
};

extern void u_hook_add(const char *name, u_hook_fn_t *fn, void *priv);
extern void u_hook_delete(const char *name, u_hook_fn_t *fn, void *priv);

/* see hook.c for more in-depth explanations of the different ways to
   invoke a hook */
extern void u_hook_call(const char *name, void *arg);
extern void *u_hook_first(const char *name, void *arg);
extern mowgli_list_t *u_hook_all(const char *name, void *arg);
extern void u_hook_all_cleanup(mowgli_list_t*);

extern int init_hook(void);

#endif

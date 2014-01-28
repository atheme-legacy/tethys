/* Tethys, module.h -- modules
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_MODULE_H__
#define __INC_MODULE_H__

#define MODULE_ABI_MAJOR 1
#define MODULE_ABI_MINOR 1

#define MODULE_LOADED       0x00000001
#define MODULE_PERMANENT    0x00000002

#define HOOK_MODULE_LOAD    "module:load"
#define HOOK_MODULE_UNLOAD  "module:unload"

typedef struct u_module u_module;
typedef struct u_module_info u_module_info;

#include "msg.h"

struct u_module {
	u_module_info *info;
	mowgli_module_t module;

	uint flags;
};

struct u_module_info {
	const char *name;
	const char *author;
	const char *description;

	short abi_major;
	short abi_minor;

	int (*init)(u_module*);
	void (*deinit)(u_module*);

	u_cmd *cmdtab;
};

#define TETHYS_MODULE_V1(NAME, AUTHOR, DESC, INIT, DEINIT, CMDTAB)          \
	u_module_info __module_info = {                                     \
		.name = NAME,                                               \
		.author = AUTHOR,                                           \
		.description = DESC,                                        \
                                                                            \
		.abi_major = 1,                                             \
		.abi_minor = 1,                                             \
                                                                            \
		.init = INIT,                                               \
		.deinit = DEINIT,                                           \
                                                                            \
		.cmdtab = CMDTAB                                            \
	}

extern u_module *u_module_loading(void);

extern mowgli_patricia_t *u_modules;
extern u_module *u_module_load(const char *name);
extern u_module *u_module_find(const char *name);
extern u_module *u_module_find_or_load(const char *name);
extern bool u_module_unload(const char *name);
extern u_module *u_module_reload(const char *name);
extern u_module *u_module_reload_or_load(const char *name);

extern void u_module_load_directory(const char *dir);

extern int init_module(void);

#endif

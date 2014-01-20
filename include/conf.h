/* Tethys, conf.h -- configuration parser
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_CONF_H__
#define __INC_CONF_H__

#define HOOK_CONF_START "conf:start"
#define HOOK_CONF_END   "conf:end"

#define U_CONF_MAX_KEY 128
#define U_CONF_MAX_VALUE 512

extern bool u_conf_read(const char *path);

/*
 * cb is called with pointers to the config file and config parsetree node.
 * The cb is loaded from u_conf_handlers using the key as the key.
 */

typedef void (u_conf_handler_t)(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce);

extern void u_conf_add_handler(const char *key, u_conf_handler_t *cb, mowgli_patricia_t *handlers);
extern void u_conf_traverse(mowgli_config_file_t *cf, mowgli_config_file_entry_t *ce, mowgli_patricia_t *handlers);
extern int init_conf(void);

#endif

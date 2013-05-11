/* ircd-micro, upgrade.h -- self-exec
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_UPGRADE_H__
#define __INC_UPGRADE_H__

typedef struct u_udb u_udb;

/* save hook args: u_udb*
   load hook args: u_udb* */

extern u_list *u_udb_save_hooks;
extern u_trie *u_udb_load_hooks;

/* save functions: */
extern void u_udb_row_start(); /* u_udb*, char *name */
extern void u_udb_row_end(); /* u_udb* */
extern void u_udb_put_s(); /* u_udb*, char *s */
extern void u_udb_put_i(); /* u_udb*, long n */

/* load functions: */
extern char *u_udb_get_s(); /* u_udb* */
extern long u_udb_get_i(); /* u_udb* */

extern int init_upgrade();

#endif

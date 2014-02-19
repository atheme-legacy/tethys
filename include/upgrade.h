/* Tethys, upgrade.h -- self-exec
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_UPGRADE_H__
#define __INC_UPGRADE_H__

extern const char *opt_upgrade;
extern mowgli_json_t *upgrade_json;

extern int init_upgrade(void);
extern int begin_upgrade(void);
extern int finish_upgrade(void);
extern void abort_upgrade(void);

#define UPGRADE_FILENAME     "upgrade.json"
#define HOOK_UPGRADE_DUMP    "upgrade:dump"
#define HOOK_UPGRADE_RESTORE "upgrade:restore"

#endif

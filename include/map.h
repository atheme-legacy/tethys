/* ircd-micro, map.h -- directional pointer pairing
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_MAP_H__
#define __INC_MAP_H__

typedef struct u_map u_map;
/* defined internally */
typedef struct u_map_n u_map_n;

#define MAP_TRAVERSING  1
#define MAP_STRING_KEYS 2

struct u_map {
	uint flags;
	u_map_n *root;
	uint size;
	u_list pending;
};

typedef void (u_map_cb_t)(u_map*, void *k, void *v, void *priv);

extern u_map *u_map_new(int string_keys);
extern void u_map_free(u_map*);
extern void u_map_each(u_map*, u_map_cb_t*, void *priv);
extern void *u_map_get(u_map*, void *key);
extern void u_map_set(u_map*, void *key, void *data);
extern void *u_map_del(u_map*, void *key);
extern void u_map_dump(u_map*);

#endif

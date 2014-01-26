/* Tethys, map.h -- directional pointer pairing
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_MAP_H__
#define __INC_MAP_H__

typedef struct u_map u_map;
typedef struct u_map_n u_map_n; /* defined internally */

#define MAP_TRAVERSING  1
#define MAP_STRING_KEYS 2

struct u_map {
	uint flags;
	u_map_n *root;
	uint size;
	mowgli_list_t pending;
};

typedef void (u_map_cb_t)(u_map*, void *k, void *v, void *priv);

extern u_map *u_map_new(int string_keys);
extern void u_map_free(u_map*);
extern void u_map_each(u_map*, u_map_cb_t*, void *priv);
extern void *u_map_get(u_map*, void *key);
extern void u_map_set(u_map*, void *key, void *data);
extern void *u_map_del(u_map*, void *key);
extern void u_map_dump(u_map*);

typedef struct u_map_each_state u_map_each_state;

struct u_map_each_state {
	u_map *map;
	mowgli_list_t list;
};

extern void u_map_each_start(u_map_each_state*, u_map*);
extern bool u_map_each_next(u_map_each_state*, void **k, void **v);

#define U_MAP_EACH(STATE, MAP, K, V) \
	for (u_map_each_start((STATE), (MAP)); \
	     u_map_each_next((STATE), (void**) &(K), &(V)); )

#endif

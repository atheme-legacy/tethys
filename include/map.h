/* ircd-micro, map.h -- directional pointer pairing
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_MAP_H__
#define __INC_MAP_H__

typedef struct u_map u_map;
/* defined internally */
typedef struct u_map_n u_map_n;

struct u_map {
	int traversing;
	u_map_n *root;
	uint size;
};

extern u_map *u_map_new();
extern void u_map_free(); /* u_map* */
/* u_map*, void (*cb)(u_map*, void *k, void *v, void*), void* */
extern void u_map_each();
extern void *u_map_get(); /* u_map*, void *key */
extern void u_map_set(); /* u_map*, void *key, void *data */
extern void *u_map_del(); /* u_map*, void *key */
extern void u_map_dump(); /* u_map* */

#endif

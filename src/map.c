/* Tethys, map.c -- AA tree
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

/* Most of this implementation was taken from Wikipedia's page on AA trees */

#include "ircd.h"

#define LEFT 0
#define RIGHT 1

struct u_map_n {
	void *key, *data;
	int level;
	u_map_n *child[2];
};

static void n_key(u_map *map, u_map_n *n, void *k)
{
	if (map->flags & MAP_STRING_KEYS) {
		if (n->key != NULL)
			free(n->key);

		if (k != NULL) {
			n->key = strdup(k); /* ;_; */
			return;
		}
	}

	n->key = k;
}

static int n_cmp(u_map *map, void *k1, void *k2)
{
	if (map->flags & MAP_STRING_KEYS)
		return strcmp((char*)k1, (char*)k2);

	return (long)k1 - (long)k2;
}

static void *n_clone(u_map *map, void *k)
{
	if (map->flags & MAP_STRING_KEYS)
		return strdup(k);

	return k;
}

static void n_free(u_map *map, void *k)
{
	if (map->flags & MAP_STRING_KEYS)
		free(k);
}

static u_map_n *u_map_n_new(u_map *map, void *key, void *data, int level)
{
	u_map_n *n;

	n = malloc(sizeof(*n));
	n->key = NULL;
	n_key(map, n, key);
	n->data = data;
	n->level = level;

	n->child[0] = n->child[1] = NULL;

	return n;
}

static void u_map_n_del(u_map *map, u_map_n *n)
{
	if ((map->flags & MAP_STRING_KEYS) && n->key)
		free(n->key);
	free(n);
}

u_map *u_map_new(int string_keys)
{
	u_map *map;

	map = malloc(sizeof(*map));
	if (map == NULL)
		return NULL;

	map->flags = string_keys ? MAP_STRING_KEYS : 0;
	map->root = NULL;
	map->size = 0;

	return map;
}

void u_map_free_n(u_map *map, u_map_n *n)
{
	if (!n)
		return;

	u_map_free_n(map, n->child[LEFT]);
	u_map_free_n(map, n->child[RIGHT]);

	u_map_n_del(map, n);
}

void u_map_free(u_map *map)
{
	u_map_free_n(map, map->root);

	free(map);
}

static u_map_n *dumb_fetch(u_map *map, void *key);
static u_map_n *aa_delete(u_map *map, u_map_n *tree, void *key);

static void clear_pending(u_map *map)
{
	mowgli_list_init(&map->pending);
}

static void delete_pending(u_map *map)
{
	mowgli_node_t *cur, *tn;

	MOWGLI_LIST_FOREACH_SAFE(cur, tn, map->pending.head) {
		u_log(LG_FINE, "DEL PENDING %p", cur->data);
		map->root = aa_delete(map, map->root, cur->data);
		n_free(map, cur->data);
		mowgli_list_delete(cur, &map->pending);
	}
}

static void add_pending(u_map *map, void *key)
{
	u_log(LG_FINE, "ADD PENDING %p", key);
	mowgli_list_add(&map->pending, n_clone(map, key));
}

static void u_map_each_n(u_map *map, u_map_n *n, u_map_cb_t *cb, void *priv)
{
	if (!n) return;

	u_map_each_n(map, n->child[LEFT], cb, priv);
	cb(map, n->key, n->data, priv);
	u_map_each_n(map, n->child[RIGHT], cb, priv);
}

void u_map_each(u_map *map, u_map_cb_t *cb, void *priv)
{
	map->flags |= MAP_TRAVERSING;
	clear_pending(map);

	u_map_each_n(map, map->root, cb, priv);

	map->flags &= ~MAP_TRAVERSING;
	delete_pending(map);
}

/* dumb functions are just standard binary search tree operations that
   don't pay attention to the colors of the nodes */

static u_map_n *dumb_fetch(u_map *map, void *key)
{
	u_map_n *n = map->root;

	while (n != NULL) {
		if (!n_cmp(map, n->key, key))
			break;
		n = n->child[n_cmp(map, n->key, key) < 0];
	}

	return n;
}

static u_map_n *skew(u_map_n *n)
{
	if (n == NULL)
		return NULL;

	if (n->child[LEFT] == NULL)
		return n;

	if (n->child[LEFT]->level == n->level) {
		u_map_n *m = n->child[LEFT];
		n->child[LEFT] = m->child[RIGHT];
		m->child[RIGHT] = n;
		return m;
	}

	return n;
}

static u_map_n *split(u_map_n *n)
{
	if (n == NULL)
		return NULL;

	if (n->child[RIGHT] == NULL || n->child[RIGHT]->child[RIGHT] == NULL)
		return n;

	if (n->level == n->child[RIGHT]->child[RIGHT]->level) {
		u_map_n *m = n->child[RIGHT];
		n->child[RIGHT] = m->child[LEFT];
		m->child[LEFT] = n;
		m->level++;
		return m;
	}

	return n;
}

static u_map_n *next(u_map_n *n, int dir)
{
	n = n->child[dir];
	while (n && n->child[!dir])
		n = n->child[!dir];
	return n;
}

static void decrease_level(u_map_n *tree)
{
	int should_be;
	u_map_n *L, *R;

	L = tree->child[LEFT];
	R = tree->child[RIGHT];

	if (L == NULL) {
		should_be = R ? R->level : 0;
	} else if (R == NULL) {
		should_be = L->level;
	} else {
		should_be = L->level < R->level ? L->level : R->level;
	}
	should_be++;

	if (should_be < tree->level) {
		tree->level = should_be;
		if (R && should_be < R->level)
			R->level = should_be;
	}
}

static u_map_n *aa_insert(u_map *map, u_map_n *tree, u_map_n *n)
{
	int c;

	if (tree == NULL)
		return n;

	c = n_cmp(map, tree->key, n->key) < 0 ? RIGHT : LEFT;
	tree->child[c] = aa_insert(map, tree->child[c], n);

	tree = skew(tree);
	tree = split(tree);

	return tree;
}

static u_map_n *aa_delete(u_map *map, u_map_n *tree, void *k)
{
	u_map_n *n;
	int c;

	if (tree == NULL)
		return NULL;

	c = n_cmp(map, tree->key, k);

	if (c != 0) {
		c = c < 0 ? RIGHT : LEFT;
		tree->child[c] = aa_delete(map, tree->child[c], k);
	} else {
		if (!tree->child[LEFT] && !tree->child[RIGHT]) {
			u_map_n_del(map, tree);
			return NULL;
		}

		c = !tree->child[LEFT] ? RIGHT : LEFT;
		n = next(tree, c);
		n_key(map, tree, n->key);
		n_key(map, n, k); /* HEHEHE! */
		tree->data = n->data;
		tree->child[c] = aa_delete(map, tree->child[c], k);
	}

	decrease_level(tree);

	tree = skew(tree);
	tree->child[RIGHT] = skew(tree->child[RIGHT]);
	if (tree->child[RIGHT] != NULL) {
		tree->child[RIGHT]->child[RIGHT] =
		    skew(tree->child[RIGHT]->child[RIGHT]);
	}
	tree = split(tree);
	tree->child[RIGHT] = split(tree->child[RIGHT]);

	return tree;
}

void *u_map_get(u_map *map, void *key)
{
	u_map_n *n = dumb_fetch(map, key);
	return n == NULL ? NULL : n->data;
}

void u_map_set(u_map *map, void *key, void *data)
{
	u_map_n *n = dumb_fetch(map, key);

	if (map->flags & MAP_TRAVERSING)
		abort();

	if (n != NULL) {
		n->data = data;
		return;
	}

	map->size++;

	n = u_map_n_new(map, key, data, 1);
	map->root = aa_insert(map, map->root, n);
}

void *u_map_del(u_map *map, void *key)
{
	u_map_n *n = dumb_fetch(map, key);
	void *data;

	if (n == NULL)
		return NULL;

	if (map->flags & MAP_STRING_KEYS)
		u_log(LG_FINE, "MAP: %p DEL %s", map, n->key);
	else
		u_log(LG_FINE, "MAP: %p DEL %p", map, n->key);

	/* we decrement size here, even if the deletion doesn't actually
	   happen, since we consider the node to have been deleted when
	   u_map_del is called. */
	map->size--;

	data = n->data;
	n->data = NULL;

	if (map->flags & MAP_TRAVERSING) {
		add_pending(map, key);
	} else {
		map->root = aa_delete(map, map->root, key);
	}

	return data;
}

static void indent(int depth)
{
	while (depth-->0)
		fprintf(stderr, "  ");
}

static void map_dump_real(u_map *map, u_map_n *n, int depth)
{
	if (n == NULL) {
		fprintf(stderr, "*");
		return;
	}

	if (map->flags & MAP_STRING_KEYS) {
		fprintf(stderr, "%s=%p (%d)\e[0m[", (char*)n->key,
		        n->data, n->level);
	} else {
		fprintf(stderr, "%p=%p (%d)\e[0m[", n->key, n->data, n->level);
	}

	if (n->child[LEFT] == NULL && n->child[RIGHT] == NULL) {
		fprintf(stderr, "]");
		return;
	}
	fprintf(stderr, "\n");
	indent(depth);
	map_dump_real(map, n->child[LEFT], depth + 1);
	fprintf(stderr, ",\n");
	indent(depth);
	map_dump_real(map, n->child[RIGHT], depth + 1);
	fprintf(stderr, "]");
}

void u_map_dump(u_map *map)
{
	map_dump_real(map, map->root, 1);
	fprintf(stderr, "\n");
}

#include "ircd.h"

#define LEFT 0
#define RIGHT 1

struct u_map_n {
	void *key, *data;
	enum u_map_color { RED, BLACK } color;
	u_map_n *parent, *child[2];
};

static u_map_n *u_map_n_new(key, data, color)
void *key, *data;
{
	u_map_n *n;

	n = malloc(sizeof(*n));
	n->key = key;
	n->data = data;
	n->color = color;

	n->parent = NULL;
	n->child[0] = n->child[1] = NULL;

	return n;
}

static void u_map_n_del(n)
u_map_n *n;
{
	free(n);
}

u_map *u_map_new()
{
	u_map *map;

	map = malloc(sizeof(*map));
	if (map == NULL)
		return NULL;

	map->traversing = 0;
	map->root = NULL;
	map->size = 0;

	return map;
}

void u_map_free(map)
u_map *map;
{
	u_map_n *n, *tn;

	for (n=map->root; n; ) {
		if (n->child[LEFT]) {
			n = n->child[LEFT];
			continue;
		}

		if (n->child[RIGHT]) {
			n = n->child[RIGHT];
			continue;
		}

		tn = n->parent;
		if (tn != NULL)
			tn->child[n == tn->child[LEFT] ? LEFT : RIGHT] = NULL;
		u_map_n_del(n);
		n = tn;
	}

	free(map);
}

/* I'm inclined to claim this tree traversal algorithm won't complain
   if you try to delete the node you're currently visiting (or any
   other node, for that matter). */
void u_map_each(map, cb, priv)
u_map *map;
void (*cb)();
void *priv;
{
	u_map_n *cur, *child, cpy;
	int idx;

	if ((cur = map->root) == NULL)
		return;

	map->traversing = 1;

try_left:
	if (cur->child[LEFT] != NULL) {
		cur = cur->child[LEFT];
		goto try_left;
	}

loop_top:
	/* we copy the current node in case it is deleted */
	memcpy(&cpy, cur, sizeof(cpy));
	cb(map, cur->key, cur->data, priv);
	child = cur;
	cur = &cpy;

	if (cur->child[RIGHT] != NULL) {
		cur = cur->child[RIGHT];
		goto try_left;
	}

	for (;;) {
		if (cur->parent == NULL)
			break;
		idx = cur->parent->child[LEFT] == child ? LEFT : RIGHT;
		child = cur = cur->parent;
		if (idx == LEFT)
			goto loop_top;
	}

	map->traversing = 0;
}

/* dumb functions are just standard binary search tree operations that
   don't pay attention to the colors of the nodes */

static u_map_n *dumb_fetch(map, key)
u_map *map;
void *key;
{
	u_map_n *n = map->root;

	while (n != NULL) {
		if (n->key == key)
			break;
		n = n->child[n->key < key];
	}

	return n;
}

static void dumb_insert(map, n)
u_map *map;
u_map_n *n;
{
	u_map_n *cur;
	int idx;

	if (map->root == NULL) {
		map->root = n;
		map->root->parent = NULL;
		return;
	}

	cur = map->root;

	for (;;) {
		idx = cur->key < n->key ? RIGHT : LEFT;
		if (cur->child[idx] == NULL) {
			cur->child[idx] = n;
			n->parent = cur;
			break;
		}
		cur = cur->child[idx];
	}
}

static u_map_n *leftmost_subchild(n)
u_map_n *n;
{
	while (n && n->child[LEFT])
		n = n->child[LEFT];
	return n;
}

static u_map_n *dumb_delete(map, n)
u_map *map;
u_map_n *n;
{
	int idx;
	u_map_n *tgt;

	if (n->child[LEFT] == NULL && n->child[RIGHT] == NULL) {
		if (n->parent == NULL) {
			/* we are the sole node, the root */
			map->root = NULL;
			return n;
		}

		idx = n->parent->child[LEFT] == n ? LEFT : RIGHT;
		n->parent->child[idx] = NULL;
		return n;

	} else if (n->child[LEFT] == NULL || n->child[RIGHT] == NULL) {
		/* tgt = the non-null child */
		tgt = n->child[n->child[LEFT] == NULL ? RIGHT : LEFT];

		if (n->parent == NULL) {
			map->root = tgt;
			tgt->parent = NULL;
			return n;
		}

		idx = n->parent->child[LEFT] == n ? LEFT : RIGHT;

		n->parent->child[idx] = tgt;
		tgt->parent = n->parent;
		return n;
	}

	/* else, both non-null */

	/* the successor will have at most one non-null child, so this
	   will only recurse once */
	tgt = leftmost_subchild(n->child[RIGHT]);
	n->data = tgt->data;
	n->key = tgt->key;
	return dumb_delete(map, tgt);
}

void *u_map_get(map, key)
u_map *map;
void *key;
{
	u_map_n *n = dumb_fetch(map, key);
	return n == NULL ? NULL : n->data;
}

void u_map_set(map, key, data)
u_map *map;
void *key, *data;
{
	u_map_n *n = dumb_fetch(map, key);

	if (n != NULL) {
		n->data = data;
		return;
	}

	map->size++;

	n = u_map_n_new(key, data, RED);
	dumb_insert(map, n);

	/* TODO: rb cases */
}

void *u_map_del(map, key)
u_map *map;
void *key;
{
	u_map_n *s, *n = dumb_fetch(map, key);
	void *data;

	if (n == NULL)
		return NULL;

	if (map->traversing)
		abort();

	map->size--;

	data = n->data;
	s = dumb_delete(map, n);

	/* TODO: rb cases */

	u_map_n_del(s);
	return data;
}

static void indent(depth)
{
	while (depth-->0)
		printf("  ");
}

static void map_dump_real(n, depth)
u_map_n *n;
int depth;
{
	if (n == NULL) {
		printf("*");
		return;
	}
	printf("\e[%sm%d=%d\e[0m[",
	       n->color == RED ? "31;1" : "36;1",
	       (int)n->key, (int)n->data);
	if (n->child[LEFT] == NULL && n->child[RIGHT] == NULL) {
		printf("]");
		return;
	}
	printf("\n");
	indent(depth);
	map_dump_real(n->child[LEFT], depth + 1);
	printf(",\n");
	indent(depth);
	map_dump_real(n->child[RIGHT], depth + 1);
	printf("]");
}

void u_map_dump(map)
u_map *map;
{
	map_dump_real(map->root, 1);
	printf("\n\n");
}

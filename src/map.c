#include "ircd.h"

#define LEFT 0
#define RIGHT 1

struct u_map_n {
	void *key, *data;
	enum u_map_color { RED, BLACK } color;
	struct u_map_n *parent, *child[2];
};

static struct u_map_n *u_map_n_new(key, data, color)
void *key, *data;
{
	struct u_map_n *n;

	n = malloc(sizeof(*n));
	n->key = key;
	n->data = data;
	n->color = color;

	n->parent = NULL;
	n->child[0] = n->child[1] = NULL;

	return n;
}

static void u_map_n_del(n)
struct u_map_n *n;
{
	free(n);
}

struct u_map *u_map_new()
{
	struct u_map *map;

	map = malloc(sizeof(*map));
	if (map == NULL)
		return NULL;

	map->root = NULL;
	map->size++;

	return map;
}

/* dumb functions are just standard binary search tree operations that
   don't pay attention to the colors of the nodes */

static struct u_map_n *dumb_fetch(map, key)
struct u_map *map;
void *key;
{
	struct u_map_n *n = map->root;

	while (n != NULL) {
		if (n->key == key)
			break;
		n = n->child[n->key < key];
	}

	return n;
}

static void dumb_insert(map, n)
struct u_map *map;
struct u_map_n *n;
{
	struct u_map_n *cur;
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

static struct u_map_n *leftmost_subchild(n)
struct u_map_n *n;
{
	while (n && n->child[LEFT])
		n = n->child[LEFT];
	return n;
}

static struct u_map_n *dumb_delete(map, n)
struct u_map *map;
struct u_map_n *n;
{
	int idx;
	struct u_map_n *tgt;

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
struct u_map *map;
void *key;
{
	struct u_map_n *n = dumb_fetch(map, key);
	return n == NULL ? NULL : n->data;
}

void u_map_set(map, key, data)
struct u_map *map;
void *key, *data;
{
	struct u_map_n *n = dumb_fetch(map, key);

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
struct u_map *map;
void *key;
{
	struct u_map_n *s, *n = dumb_fetch(map, key);
	void *data;

	if (n == NULL)
		return NULL;

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
struct u_map_n *n;
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
struct u_map *map;
{
	map_dump_real(map->root, 1);
	printf("\n\n");
}

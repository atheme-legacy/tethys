#include "ircd.h"

#define LEFT 0
#define RIGHT 1

struct u_map_n {
	void *key, *data;
	enum u_map_color { RED, BLACK } color;
	u_map_n *parent, *child[2];
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

static u_map_n *u_map_n_new(u_map *map, void *key, void *data, int color)
{
	u_map_n *n;

	n = malloc(sizeof(*n));
	n->key = NULL;
	n_key(map, n, key);
	n->data = data;
	n->color = color;

	n->parent = NULL;
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

void u_map_free(u_map *map)
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
		u_map_n_del(map, n);
		n = tn;
	}

	free(map);
}

static u_map_n *dumb_fetch(u_map *map, void *key);
static void rb_delete(u_map *map, u_map_n *n);

static void clear_pending(u_map *map)
{
	mowgli_list_init(&map->pending);
}

static void delete_pending(u_map *map)
{
	mowgli_node_t *cur, *tn;
	u_map_n *n;

	MOWGLI_LIST_FOREACH_SAFE(cur, tn, map->pending.head) {
		n = dumb_fetch(map, cur->data);
		u_log(LG_FINE, "DEL PENDING %p (n=%p)", cur->data, n);
		if (n != NULL)
			rb_delete(map, n);
		mowgli_list_delete(cur, &map->pending);
	}
}

static void add_pending(u_map *map, u_map_n *n)
{
	u_log(LG_FINE, "ADD PENDING %p (n=%p)", n->key, n);
	mowgli_list_add(&map->pending, n->key);
}

void u_map_each(u_map *map, u_map_cb_t *cb, void *priv)
{
	u_map_n *cur;
	int idx;

	if ((cur = map->root) == NULL)
		return;

	map->flags |= MAP_TRAVERSING;
	clear_pending(map);

try_left:
	if (cur->child[LEFT] != NULL) {
		cur = cur->child[LEFT];
		goto try_left;
	}

loop_top:
	cb(map, cur->key, cur->data, priv);

	if (cur->child[RIGHT] != NULL) {
		cur = cur->child[RIGHT];
		goto try_left;
	}

	for (;;) {
		if (cur->parent == NULL)
			break;
		idx = cur->parent->child[LEFT] == cur ? LEFT : RIGHT;
		cur = cur->parent;
		if (idx == LEFT)
			goto loop_top;
	}

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

static void dumb_insert(u_map *map, u_map_n *n)
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
		idx = n_cmp(map, cur->key, n->key) < 0 ? RIGHT : LEFT;
		if (cur->child[idx] == NULL) {
			cur->child[idx] = n;
			n->parent = cur;
			break;
		}
		cur = cur->child[idx];
	}
}

static u_map_n *leftmost_subchild(u_map_n *n)
{
	while (n && n->child[LEFT])
		n = n->child[LEFT];
	return n;
}

static u_map_n *grandparent(u_map_n *n)
{
	return n && n->parent ? n->parent->parent : NULL;
}

static u_map_n *uncle(u_map_n *n)
{
	u_map_n *g = grandparent(n);
	if (g == NULL)
		return NULL;
	return n->parent == g->child[LEFT] ? g->child[RIGHT] : g->child[LEFT];
}

static u_map_n *sibling(u_map_n *n)
{
	if (n->parent == NULL)
		return NULL;

	if (n == n->parent->child[LEFT])
		return n->parent->child[RIGHT];
	else
		return n->parent->child[LEFT];
}

static void rotate(u_map *map, u_map_n *n, int dir)
{
	u_map_n *m, *b;

	if ((m = n->child[!dir]) == NULL) {
		u_log(LG_ERROR, "map: Can't rotate %s!",
		      dir == LEFT ? "left" : "right");
		return;
	}

	b = m ? m->child[dir] : NULL;

	if (n->parent) {
		if (n->parent->child[LEFT] == n)
			n->parent->child[LEFT] = m;
		else
			n->parent->child[RIGHT] = m;
	}

	m->parent = n->parent;
	n->parent = m;

	m->child[dir] = n;
	n->child[!dir] = b;

	if (b)
		b->parent = n;

	if (map->root == n)
		map->root = m;
}

static u_map_n *dumb_delete(u_map *map, u_map_n *n, u_map_n **child)
{
	int idx;
	u_map_n *tgt;

	*child = NULL;

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
		*child = tgt;

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
	/* we don't use n_key here, since this is faster */
	if ((map->flags & MAP_STRING_KEYS) && n->key)
		free(n->key);
	n->key = tgt->key;
	tgt->key = NULL;
	return dumb_delete(map, tgt, child);
}

static void rb_delete(u_map *map, u_map_n *n)
{
	u_map_n *s, *m;

	if (map->flags & MAP_STRING_KEYS)
		u_log(LG_FINE, "MAP: %p RB-DEL %s", map, n->key);
	else
		u_log(LG_FINE, "MAP: %p RB-DEL %p", map, n->key);

	m = dumb_delete(map, n, &n);

case1:
	if (m->color != BLACK || !n)
		goto finish;

	if (n->color == RED) {
		n->color = BLACK;
		goto finish;
	}

	if (!n->parent)
		goto finish;

	s = sibling(n);
	if (s->color == RED) {
		n->parent->color = RED;
		s->color = BLACK;
		if (n == n->parent->child[RIGHT])
			rotate(map, n->parent, LEFT);
		else
			rotate(map, n->parent, RIGHT);
	}

	s = sibling(n);
	if (n->parent->color == BLACK &&
	    s->color == BLACK &&
	    s->child[LEFT]->color == BLACK &&
	    s->child[RIGHT]->color == BLACK) {
		s->color = RED;
		n = n->parent;
		goto case1;
	}

	if (n->parent->color == RED &&
	    s->color == BLACK &&
	    s->child[LEFT]->color == BLACK &&
	    s->child[RIGHT]->color == BLACK) {
		s->color = RED;
		n->parent->color = BLACK;
		goto finish;
	}

	if (s->color == BLACK) {
		if (n == n->parent->child[LEFT] &&
		    s->child[LEFT]->color == RED &&
		    s->child[RIGHT]->color == BLACK) {
			s->color = RED;
			s->child[LEFT]->color = BLACK;
			rotate(map, s, RIGHT);
		} else if (n == n->parent->child[RIGHT] &&
		    s->child[LEFT]->color == BLACK &&
		    s->child[RIGHT]->color == RED) {
			s->color = RED;
			s->child[RIGHT]->color = BLACK;
			rotate(map, s, LEFT);
		}
	}
	s = sibling(n);

	s->color = n->parent->color;
	n->parent->color = BLACK;
	if (n == n->parent->child[LEFT]) {
		s->child[RIGHT]->color = BLACK;
		rotate(map, n->parent, LEFT);
	} else {
		s->child[LEFT]->color = BLACK;
		rotate(map, n->parent, RIGHT);
	}

finish:

	u_map_n_del(map, m);
}

void *u_map_get(u_map *map, void *key)
{
	u_map_n *n = dumb_fetch(map, key);
	return n == NULL ? NULL : n->data;
}

void u_map_set(u_map *map, void *key, void *data)
{
	u_map_n *n = dumb_fetch(map, key);
	u_map_n *u, *g;

	if (map->flags & MAP_TRAVERSING)
		abort();

	if (n != NULL) {
		n->data = data;
		return;
	}

	map->size++;

	n = u_map_n_new(map, key, data, RED);
	dumb_insert(map, n);

case1:
	if (n->parent == NULL) {
		n->color = BLACK;
		goto finish;
	}

	if (n->parent->color == BLACK)
		goto finish;

	u = uncle(n);
	if (u && u->color == RED) {
		n->parent->color = BLACK;
		u->color = BLACK;
		n = grandparent(n);
		n->color = RED;
		goto case1;
	}

	g = grandparent(n);
	if (n == n->parent->child[RIGHT] && n->parent == g->child[LEFT]) {
		rotate(map, n->parent, LEFT);
		n = n->child[LEFT];
	} else if (n == n->parent->child[LEFT] && n->parent == g->child[RIGHT]) {
		rotate(map, n->parent, RIGHT);
		n = n->child[RIGHT];
	}

	g = grandparent(n);
	n->parent->color = BLACK;
	g->color = RED;
	if (n == n->parent->child[LEFT])
		rotate(map, g, RIGHT);
	else
		rotate(map, g, LEFT);
finish:

	return;
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

	if (map->flags & MAP_TRAVERSING)
		add_pending(map, n);
	else
		rb_delete(map, n);

	return data;
}

static void indent(int depth)
{
	while (depth-->0)
		printf("  ");
}

static void map_dump_real(u_map *map, u_map_n *n, int depth)
{
	if (n == NULL) {
		printf("*");
		return;
	}

/*
	this section of code produces too many compiler warnings.
	uncomment if you need to dump maps.

	if (map->flags & MAP_STRING_KEYS) {
		printf("\e[%sm%s=%d\e[0m[", n->color == RED ? "31;1" : "36;1",
		       (char*)n->key, (long)n->data);
	} else {
		printf("\e[%sm%d=%d\e[0m[", n->color == RED ? "31;1" : "36;1",
		       (long)n->key, (long)n->data);
	}
*/

	if (n->child[LEFT] == NULL && n->child[RIGHT] == NULL) {
		printf("]");
		return;
	}
	printf("\n");
	indent(depth);
	map_dump_real(map, n->child[LEFT], depth + 1);
	printf(",\n");
	indent(depth);
	map_dump_real(map, n->child[RIGHT], depth + 1);
	printf("]");
}

void u_map_dump(u_map *map)
{
	map_dump_real(map, map->root, 1);
	printf("\n\n");
}

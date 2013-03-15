#include "ircd.h"

#define TRIE_E_HEAP_COUNT 128 /* needs to be tuned? */
struct u_heap *e_heap = NULL;

static void __null_canonize(s) { }

static struct u_trie_e *trie_e_new(up)
struct u_trie_e *up;
{
	struct u_trie_e *e;
	int i;

	if (e_heap == NULL)
		e_heap = u_heap_new(sizeof(*e), TRIE_E_HEAP_COUNT);

	e = u_heap_alloc(e_heap);
	e->val = NULL;
	e->up = up;
	for (i=0; i<16; i++)
		e->n[i] = NULL;

	return e;
}

static void trie_e_del(e)
struct u_trie_e *e;
{
	u_heap_free(e_heap, e);
}

struct u_trie *u_trie_new(canonize)
void (*canonize)();
{
	struct u_trie *trie;
	int i;

	trie = malloc(sizeof(*trie));
	trie->canonize = canonize ? canonize : __null_canonize;

	trie->n.val = NULL;
	trie->n.up = NULL;
	for (i=0; i<16; i++)
		trie->n.n[i] = 0;

	return trie;
}

static unsigned char nibble(s, i)
unsigned char *s;
{
	return (i%2==0) ? s[i/2]>>4 : s[i/2]&0xf;
}

static struct u_trie_e *retrieval(trie, tkey, create)
struct u_trie *trie;
char *tkey;
int create;
{
	struct u_trie_e *n;
	char key[U_TRIE_KEY_MAX];
	unsigned int c, nib = 0; /* even=high, odd=low */

	u_strlcpy(key, tkey, U_TRIE_KEY_MAX);

	trie->canonize(key);

	if (!key[0])
		return NULL;

	n = &trie->n;

	for (; n && key[nib/2]; nib++) {
		c = nibble(key, nib);
		if (n->n[c] == NULL) {
			if (create)
				n->n[c] = trie_e_new(n);
			else
				return NULL;
		}
		n = n->n[c];
	}

	return n;
}

void u_trie_set(trie, key, val)
struct u_trie *trie;
char *key;
void *val;
{
	struct u_trie_e *n = retrieval(trie, key, 1);
	n->val = val;
}

void *u_trie_get(trie, key)
struct u_trie *trie;
char *key;
{
	struct u_trie_e *n = retrieval(trie, key, 0);
	return n ? n->val : NULL;
}

void *u_trie_del(trie, key)
struct u_trie *trie;
char *key;
{
	struct u_trie_e *prev, *cur;
	void *val;
	int i, nempty;

	cur = retrieval(trie, key, 0);

	if (cur == NULL)
		return NULL;

	val = cur->val;

	prev = cur;
	cur = cur->up;
	while (cur) {
		for (i=0; i<16; i++) {
			if (cur->n[i] == prev)
				cur->n[i] = NULL;
			if (!cur->n[i])
				nempty++;
		}
		prev = cur;
		if (nempty != 16 || cur->val)
			break;
		cur = cur->up;
		trie_e_del(prev);
	}

	return val;
}

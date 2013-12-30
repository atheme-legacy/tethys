/* ircd-micro, trie.h -- radix trie
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_TRIE_H__
#define __INC_TRIE_H__

/* This is used for some internal buffers in the trie functions. Since these
   bytes are allocated on the stack, you can set this pretty high. Think of
   the longest key you might need to insert, then multiply by 4x */
#define U_TRIE_KEY_MAX 2048

typedef struct u_trie u_trie;
typedef struct u_trie_e u_trie_e;

typedef void (u_trie_canonize_t)(char *key);
typedef void (u_trie_cb_t)(void *value, void *priv);

struct u_trie_e {
	void *val;
	u_trie_e *up;
	u_trie_e *n[16];
};

struct u_trie {
	u_trie_canonize_t *canonize;
	u_trie_e n;
};

extern u_trie *u_trie_new(u_trie_canonize_t*);
extern void u_trie_free(u_trie*, u_trie_cb_t*, void *priv);
extern void u_trie_set(u_trie*, char*, void*);
extern void *u_trie_get(u_trie*, char*);
extern void u_trie_each(u_trie*, char*, u_trie_cb_t*, void*);
extern void *u_trie_del(u_trie*, char*);

#endif

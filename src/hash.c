#include "ircd.h"

int hashstr(s)
char *s;
{
	int sum = 0;
	while (*s) {
		sum = 2*sum + *s;
		s++;
	}
	return sum;
}

struct u_hash_e *hash_e_new()
{
	return (struct u_hash_e*)malloc(sizeof(struct u_hash_e));
}

void hash_e_free(he)
struct u_hash_e *he;
{
	if (he != NULL) free(he);
}

void u_hash_init(hash)
struct u_hash *hash;
{
	int i;

	hash->sz = 0;
	for (i=0; i<U_HASH_TBL_SIZE; i++)
		hash->tbl[i] = NULL;
}

void u_hash_reset(hash)
struct u_hash *hash;
{
	struct u_hash_e *he, *hen;
	int i;

	hash->sz = 0;
	for (i=0; i<U_HASH_TBL_SIZE; i++) {
		he = hash->tbl[i];
		while (he) {
			hen = he->next;
			hash_e_free(he);
			he = hen;
		}
		hash->tbl[i] = NULL;
	}
}

void u_hash_set(hash, key, val)
struct u_hash *hash;
char *key;
void *val;
{
	int h = hashstr(key) % U_HASH_TBL_SIZE;
	struct u_hash_e *he;

	he = hash->tbl[h];
	while (he) {
		if (strcmp(he->key, key) == 0) {
			he->val = val;
			return;
		}
		he = he->next;
	}

	he = hash_e_new();
	he->key = key;
	he->val = val;
	he->next = hash->tbl[h];
	hash->tbl[h] = he;
	hash->sz ++;
}

void *u_hash_get(hash, key)
struct u_hash *hash;
char *key;
{
	int h = hashstr(key) % U_HASH_TBL_SIZE;
	struct u_hash_e *he;

	he = hash->tbl[h];
	while (he) {
		if (strcmp(he->key, key) == 0)
			return he->val;
		he = he->next;
	}

	return NULL;
}

void *u_hash_del(hash, key)
struct u_hash *hash;
char *key;
{
	int h = hashstr(key) % U_HASH_TBL_SIZE;
	struct u_hash_e *cur, *prev;
	void *val;

	cur = hash->tbl[h];
	prev = NULL;
	while (cur) {
		if (strcmp(cur->key, key) == 0) {
			if (prev == NULL)
				hash->tbl[h] = cur->next;
			else
				prev->next = cur->next;
			val = cur->val;
			hash_e_free(cur);
			hash->sz --;
			return val;
		}
		prev = cur;
		cur = cur->next;
	}

	return NULL;
}

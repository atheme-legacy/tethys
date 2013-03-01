#ifndef __INC_HASH_H__
#define __INC_HASH_H__

#define U_HASH_TBL_SIZE 1024

struct u_hash_e {
	char *key;
	void *val;
	struct u_hash_e *next;
};

struct u_hash {
	unsigned sz;
	struct u_hash_e *tbl[U_HASH_TBL_SIZE];
};

extern void u_hash_init(); /* struct u_hash* */
extern void u_hash_reset(); /* struct u_hash* */
extern void u_hash_set(); /* struct u_hash*, char*, void* */
extern void *u_hash_get(); /* struct u_hash*, char* */
extern void *u_hash_del(); /* struct u_hash*, char* */

#endif

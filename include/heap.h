#ifndef __INC_HEAP_H__
#define __INC_HEAP_H__

#define U_HEAP_IN_USE 0x0001

struct u_heap_e {
	unsigned count;
	void *base;
	struct u_heap_e *next;
};

struct u_heap {
	unsigned sz, step, count;
	struct u_heap_e *start;
};

extern struct u_heap *u_heap_new(); /* unsigned sz, unsigned count */
extern void *u_heap_alloc(); /* u_heap* */
extern void u_heap_free(); /* u_heap*, void* */

#endif

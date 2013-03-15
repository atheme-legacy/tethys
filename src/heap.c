#include "ircd.h"

static struct u_heap_e *heap_e_alloc(heap)
struct u_heap *heap;
{
	struct u_heap_e *h;
	void *cur, *end;
	unsigned step;

	h = malloc(sizeof(*h));
	h->count = 0;
	h->next = NULL;
	h->base = mmap(NULL, heap->step * heap->count, PROT_READ|PROT_WRITE,
	               MAP_PRIVATE|MAP_ANON, -1, 0);

	u_log(LG_DEBUG, "NEW HEAP_E(%p): BASE=%p", h, h->base);

	end = h->base + heap->step * heap->count;

	for (cur=h->base; cur<end; cur+=heap->step)
		*((unsigned*)cur) = 0;

	return h;
} 

struct u_heap *u_heap_new(sz, count)
unsigned sz, count;
{
	struct u_heap *heap;

	heap = malloc(sizeof(*heap));
	heap->sz = sz;
	heap->count = count;
	heap->step = sizeof(unsigned) + heap->sz;
	heap->start = heap_e_alloc(heap);

	u_log(LG_DEBUG, "NEW HEAP(%p): SZ=%xx STEP=%xx COUNT=%d SZ_E=%xx", heap,
	        heap->sz, heap->step, heap->count, heap->step*heap->count);

	return heap;
}

struct u_heap_e *heap_e_find(heap)
struct u_heap *heap;
{
	struct u_heap_e *cur = heap->start;
	struct u_heap_e *prev = NULL;

	while (cur && cur->count == heap->count) {
		prev = cur;
		cur = cur->next;
	}

	if (!cur) {
		if (!prev || prev->next) /* inconsistent state */
			return NULL;
		prev->next = cur = heap_e_alloc(heap);
	}

	return cur;
}

void *u_heap_alloc(heap)
struct u_heap *heap;
{
	struct u_heap_e *h;
	void *cur, *end;
	unsigned *p;

	if (!(h = heap_e_find(heap)))
		return NULL;

	end = h->base + heap->step * heap->count;

	for (cur=h->base; cur<end; cur+=heap->step) {
		p = cur;
		if (*p & U_HEAP_IN_USE)
			continue;
		*p |= U_HEAP_IN_USE;
		h->count++;
		return cur + sizeof(*p);
	}

	return NULL;
}

void u_heap_free(heap, ptr)
struct u_heap *heap;
void *ptr;
{
	struct u_heap_e *h = heap->start;
	unsigned sz, *p;

	if (!ptr)
		return;

	sz = heap->step * heap->count;

	for (; h; h = h->next) {
		if (ptr < h->base || ptr >= h->base + sz)
			continue;
		p = ptr;
		if (p[-1] & U_HEAP_IN_USE)
			h->count--;
		p[-1] &= ~U_HEAP_IN_USE;
	}
}

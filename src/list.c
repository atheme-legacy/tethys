#include "ircd.h"

void u_list_init(list)
struct u_list *list;
{
	list->data = NULL;
	list->prev = list;
	list->next = list;
}

struct u_list *u_list_add(list, data)
struct u_list *list;
void *data;
{
	struct u_list *n;

	n = (struct u_list*)malloc(sizeof(*n));
	if (n == NULL)
		return NULL;

	n->data = data;
	n->next = list->next;
	n->prev = list;
	n->next->prev = n;
	n->prev->next = n;

	return n;
}

struct u_list *u_list_contains(list, data)
struct u_list *list;
void *data;
{
	struct u_list *n;

	U_LIST_EACH(n, list) {
		if (n->data == data)
			return n;
	}

	return NULL;
}

void *u_list_del_n(n)
struct u_list *n;
{
	void *data;
	n->next->prev = n->prev;
	n->prev->next = n->next;
	data = n->data;
	free(n);
	return data;
}

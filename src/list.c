/* ircd-micro, list.c -- linked list
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void u_list_init(list) u_list *list;
{
	list->data = (void *)0;
	list->prev = list;
	list->next = list;
}

u_list *u_list_add(list, data) u_list *list; void *data;
{
	u_list *n;

	n = malloc(sizeof(u_list));
	if (n == NULL)
		return NULL;

	n->data = data;
	n->next = list;
	n->prev = list->prev;
	n->next->prev = n;
	n->prev->next = n;

	list->data = (void*)((int)(list->data) + 1);

	return n;
}

u_list *u_list_contains(list, data) u_list *list; void *data;
{
	u_list *n;

	U_LIST_EACH(n, list) {
		if (n->data == data)
			return n;
	}

	return NULL;
}

int u_list_is_empty(list) u_list *list;
{
	return list->next == list;
} 

int u_list_size(list) u_list *list;
{
	return (int)(list->data);
}

void *u_list_del_n(list, n) u_list *list, *n;
{
	void *data;

	n->next->prev = n->prev;
	n->prev->next = n->next;
	data = n->data;

	free(n);
	list->data = (void*)((int)(list->data) - 1);

	return data;
}

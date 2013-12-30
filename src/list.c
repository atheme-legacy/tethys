/* ircd-micro, list.c -- linked list
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void u_list_init(u_list *list)
{
	list->data = (void *)0;
	list->prev = list;
	list->next = list;
}

u_list *u_list_add(u_list *list, void *data)
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

	list->data = (void*)((ulong)(list->data) + 1);

	return n;
}

u_list *u_list_contains(u_list *list, void *data)
{
	u_list *n;

	U_LIST_EACH(n, list) {
		if (n->data == data)
			return n;
	}

	return NULL;
}

int u_list_is_empty(u_list *list)
{
	return list->next == list;
} 

ulong u_list_size(u_list *list)
{
	return (ulong)(list->data);
}

void *u_list_del_n(u_list *list, u_list *n)
{
	void *data;

	n->next->prev = n->prev;
	n->prev->next = n->next;
	data = n->data;

	free(n);
	if (list->data)
		list->data = (void*)((ulong)(list->data) - 1);

	return data;
}

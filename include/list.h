/* ircd-micro, list.h -- linked list
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_LIST_H__
#define __INC_LIST_H__

typedef struct u_list u_list;

struct u_list {
	void *data;
	u_list *prev;
	u_list *next;
};

#define U_LIST_EACH(n, list) \
	for((n)=(list)->next; (n)!=(list); (n)=(n)->next)
#define U_LIST_EACH_SAFE(n, tn, list) \
	for ((n)=(list)->next, (tn)=(n)->next; (n)!=(list); \
	     (n)=(tn), (tn)=(n)->next)

extern void u_list_init(); /* u_list* */
extern u_list *u_list_add(); /* u_list*, void* */
extern u_list *u_list_contains(); /* u_list*, void* */
extern int u_list_is_empty(); /* u_list* */
extern ulong u_list_size(); /* u_list* */
extern void *u_list_del_n(); /* u_list *list, *node */

#endif

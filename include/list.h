#ifndef __INC_LIST_H__
#define __INC_LIST_H__

struct u_list {
	void *data;
	struct u_list *prev;
	struct u_list *next;
};

#define U_LIST_EACH(n, list) \
	for((n)=(list)->next; (n)!=(list); (n)=(n)->next)
#define U_LIST_EACH_SAFE(n, tn, list) \
	for ((n)=(list)->next, (tn)=(n)->next; (n)!=(list); \
	     (n)=(tn), (tn)=(n)->next)

extern void u_list_init(); /* struct u_list* */
extern struct u_list *u_list_add(); /* struct u_list*, void* */
extern struct u_list *u_list_contains(); /* struct u_list*, void* */
extern int u_list_is_empty(); /* struct u_list* */
extern void *u_list_del_n(); /* struct u_list *node */

#endif

/* ircd-micro, entity.h -- generic entity targeting
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_ENTITY_H__
#define __INC_ENTITY_H__

typedef struct u_entity u_entity;

#define ENT_TYPE_MASK       0x000000ff
#define ENT_USER            0x00000001
#define ENT_SERVER          0x00000002

#define ENT_TYPE(e) ((e)->flags & ENT_TYPE_MASK)

#define ENT_IS_USER(e) (ENT_TYPE(e) == ENT_USER)
#define ENT_IS_SERVER(e) (ENT_TYPE(e) == ENT_SERVER)
#define ENT_IS_LOCAL(e) ((e)->loc != NULL)
#define ENT_IS_REMOTE(e) ((e)->loc == NULL)

struct u_entity {
	uint flags;

	char *name;
	char *id;

	u_conn *link;
	u_conn *loc;

	union {
		void *p;
		u_user *u;
		u_user_local *ul;
		u_user_remote *ur;
		u_server *sv;
	} v;
};

extern u_entity *u_entity_from_name(); /* u_entity *e, char *name */
extern u_entity *u_entity_from_id(); /* u_entity *e, char *id */
extern u_entity *u_entity_from_ref(); /* u_entity *e, char *ref */

extern u_entity *u_entity_from_user(); /* u_entity *e, u_user *u */
extern u_entity *u_entity_from_server(); /* u_entity *e, u_server *sv */

#endif

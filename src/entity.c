/* ircd-micro, entity.c -- generic entity targeting
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void make_server(e) u_entity *e;
{
	u_server *sv = e->v.sv;

	e->flags = ENT_SERVER;

	e->name = sv->name;
	e->id = sv->sid;

	e->link = sv->conn;
	e->loc = NULL;
	if (sv->hops == 1)
		e->loc = e->link;
}

void make_user(e) u_entity *e;
{
	u_user *u = e->v.u;

	e->flags = ENT_USER;

	e->name = u->nick;
	e->id = u->uid;

	if (u->flags & USER_IS_LOCAL) {
		e->loc = USER_LOCAL(u)->conn;
		e->link = e->loc;
	} else {
		e->loc = NULL;
		e->link = USER_REMOTE(u)->server->conn;
	}
}

int u_entity_from_name(e, s) u_entity *e; char *s;
{
	if (strchr(s, '.')) {
		if (!(e->v.sv = u_server_by_name(s)))
			return -1;
		make_server(e);
	} else {
		if (!(e->v.u = u_user_by_nick(s)))
			return -1;
		make_user(e);
	}

	return 0;
}

int u_entity_from_id(e, s) u_entity *e; char *s;
{
	if (s[4]) {
		if (!(e->v.u = u_user_by_uid(s)))
			return -1;
		make_user(e);
	} else {
		if (!(e->v.sv = u_server_by_name(s)))
			return -1;
		make_server(e);
	}

	return 0;
}

int u_entity_from_ref(e, s) u_entity *e; char *s;
{
	return u_entity_from_id(e, ref_to_id(s));
}

int u_entity_from_user(e, u) u_entity *e; u_user *u;
{
	e->v.u = u;
	make_user(e);
	return 0;
}

int u_entity_from_server(e, sv) u_entity *e; u_server *sv;
{
	e->v.sv = sv;
	make_server(e);
	return 0;
}

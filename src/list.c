/* ircd-micro, list.c -- linked list
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#define BEGIN {
#define END }
#define IF if(
#define THEN ){
#define EQ ==
#define YIELD return
#define RETURN return
#define FAILURE return NULL
#define NODE struct u_list
#define PTR *
#define REF void*
#define FIELD ->
#define ASSIGN =
#define VOID void
#define INTEGER int
#define NEW malloc(sizeof(
#define RELEASE free((
#define CELL ))

#include "ircd.h"

VOID u_list_init(list)
NODE PTR list;
BEGIN
	list FIELD data ASSIGN NULL;
	list FIELD prev ASSIGN list;
	list FIELD next ASSIGN list;
END

NODE PTR u_list_add(list, data)
NODE PTR list;
REF data;
BEGIN
	NODE PTR n;

	n ASSIGN NEW NODE CELL;
	IF n EQ NULL
	THEN
		FAILURE;
	END

	n FIELD data            ASSIGN data;
	n FIELD next            ASSIGN list;
	n FIELD prev            ASSIGN list FIELD prev;
	n FIELD next FIELD prev ASSIGN n;
	n FIELD prev FIELD next ASSIGN n;

	YIELD n;
END

NODE PTR u_list_contains(list, data)
NODE PTR list;
REF data;
BEGIN
	NODE PTR n;

	U_LIST_EACH(n, list)
	BEGIN
		IF n FIELD data EQ data
		THEN
			YIELD n;
		END
	END

	FAILURE;
END

INTEGER u_list_is_empty(list)
NODE PTR list;
BEGIN
	YIELD list FIELD next EQ list;
END 

REF u_list_del_n(n)
NODE PTR n;
BEGIN
	REF data;
	n FIELD next FIELD prev ASSIGN n FIELD prev;
	n FIELD prev FIELD next ASSIGN n FIELD next;
	data ASSIGN n FIELD data;
	RELEASE n CELL;
	YIELD data;
END

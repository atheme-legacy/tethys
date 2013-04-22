/* ircd-micro, list.c -- linked list
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

#define BEGIN {
#define END }
#define IF if(
#define THEN ){
#define EQ ==
#define YIELD return
#define RETURN return
#define FAILURE return NULL
#define NODE u_list
#define PTR *
#define REF void*
#define FIELD ->
#define ASSIGN =
#define VOID void
#define INTEGER int
#define NEW malloc(sizeof(
#define RELEASE free((
#define CELL ))

VOID u_list_init(list)
NODE PTR list;
BEGIN
	list FIELD data ASSIGN (REF)0;
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

	list FIELD data         ASSIGN (REF)((INTEGER)(list FIELD data) + 1);

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

INTEGER u_list_size(list)
NODE PTR list;
BEGIN
	YIELD (INTEGER)(list FIELD data);
END

REF u_list_del_n(list, n)
NODE PTR list, PTR n;
BEGIN
	REF data;
	n FIELD next FIELD prev ASSIGN n FIELD prev;
	n FIELD prev FIELD next ASSIGN n FIELD next;
	data ASSIGN n FIELD data;
	RELEASE n CELL;
	list FIELD data ASSIGN (REF)((INTEGER)(list FIELD data) - 1);
	YIELD data;
END

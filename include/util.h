/* ircd-micro, util.h -- various utilities
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_UTIL_H__
#define __INC_UTIL_H__

extern int matchmap(); /* char *pat, char *string, char *map */
extern int match(); /* char *pattern, char *string */
extern int matchirc(); /* rfc1459 casemapping */
extern int matchcase(); /* ascii casemapping */

extern void u_memmove();
extern void u_strlcpy();
extern void u_strlcat();
extern char *u_strdup();
extern void u_ntop(); /* in_addr*, char* */

extern char *cut(); /* char **p, char *delim */

extern void null_canonize();
extern void rfc1459_canonize();
extern void ascii_canonize();

extern int is_valid_nick();
extern int is_valid_ident();

extern int init_util();

#endif

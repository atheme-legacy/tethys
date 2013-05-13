/* ircd-micro, util.h -- various utilities
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_UTIL_H__
#define __INC_UTIL_H__

#define CIDR_ADDRSTRLEN (INET_ADDRSTRLEN+3)

typedef struct u_cidr u_cidr;

struct u_cidr {
	ulong addr;
	int netsize;
};

extern void u_cidr_to_str(); /* u_cidr*, char* */
extern void u_str_to_cidr(); /* char*, u_cidr* */
extern int u_cidr_match(); /* u_cidr*, char* */

extern int matchmap(); /* char *pat, char *string, char *map */
extern int match(); /* char *pattern, char *string */
extern int matchirc(); /* rfc1459 casemapping */
extern int matchcase(); /* ascii casemapping */

extern int matchhash(); /* char *hash, char *string */

#define streq(a,b) (!strcmp((a),(b)))

extern int mapcmp(); /* char *s1, char *s2, char *map */
extern int casecmp(); /* char *s1, char *s2 */
extern int irccmp(); /* char *s1, char *s2 */

extern void u_memmove();
extern void u_strlcpy();
extern void u_strlcat();
extern char *u_strdup();
extern void u_ntop(); /* in_addr*, char* */
extern void u_aton(); /* char*, in_addr* */

extern char *cut(); /* char **p, char *delim */
extern int wrap(); /* char *base, char **p, uint w, char *str */

extern void null_canonize();
extern void rfc1459_canonize();
extern void ascii_canonize();

extern char *id_to_name(); /* char *sid_or_uid */
extern char *name_to_id(); /* char *nick_or_server */
extern char *ref_to_name(); /* char *ref */
extern char *ref_to_id(); /* char *ref */

extern int is_valid_nick();
extern int is_valid_ident();
extern int is_valid_sid();

extern int init_util();

#endif

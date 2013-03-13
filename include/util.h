#ifndef __INC_UTIL_H__
#define __INC_UTIL_H__

extern int match(); /* char *pattern, char *string */

extern void u_memmove();
extern void u_strlcpy();

extern void rfc1459_canonize();
extern int is_valid_nick();
extern int is_valid_ident();

extern int init_util();

#endif

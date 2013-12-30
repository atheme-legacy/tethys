/* ircd-micro, dns.h -- domain name lookups
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_DNS_H__
#define __INC_DNS_H__

/* callback statuses */
#define DNS_OKAY      0   /* request was okay, res is valid */
#define DNS_NXDOMAIN  1   /* nonexistent domain */
#define DNS_TIMEOUT   2   /* request timed out */
#define DNS_INVALID   3   /* invalid request */
#define DNS_TOOLONG   4   /* request too long */
#define DNS_OTHER     5   /* some other error */

extern void u_dns_use_io(u_io*);

typedef void (u_dns_cb_t)(int status, char *res, void *priv);

extern ushort u_dns(char*, u_dns_cb_t*, void *priv);
extern ushort u_rdns(char*, u_dns_cb_t*, void *priv);

extern void u_dns_cancel(ushort, u_dns_cb_t*, void *priv);

extern int init_dns(void);

#endif

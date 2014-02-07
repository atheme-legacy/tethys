/* Tethys, user.h -- user management
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_USER_H__
#define __INC_USER_H__

/* There's no real rationale behind these values other
   than "they looked good" */
#define MAXNICKLEN 20
#define MAXACCOUNT 20
#define MAXIDENT 16
#define MAXHOST 120
#define MAXGECOS 40

#define MAXAWAY 256

#define UMODE_OPER             0x00000001
#define UMODE_INVISIBLE        0x00000002
#define UMODE_WALLOPS          0x00000004
#define UMODE_CLOAKED          0x00000008
#define UMODE_SERVICE          0x00000010

/* all users */
#define USER_MASK_FLAGS        0x000000ff
#define USER_IS_LOCAL          0x00000001

/* local */
#define USER_MASK_CAP          0x0000ff00
#define CAP_MULTI_PREFIX       0x00000100
#define CAP_AWAY_NOTIFY        0x00000200

/* registration postpone */
#define USER_MASK_WAIT         0x00ff0000
#define USER_WAIT_CAPS         0x00010000

typedef struct u_user u_user;

#include "conn.h"
#include "server.h"
#include "mode.h"

struct u_user {
	char uid[10];

	uint mode, flags;
	u_map *channels;
	u_map *invites;

	char nick[MAXNICKLEN+1];
	char acct[MAXACCOUNT+1];
	u_ts_t nickts;

	char ident[MAXIDENT+1];

	char ip[INET_ADDRSTRLEN];
	char realhost[MAXHOST+1];
	char host[MAXHOST+1];

	char gecos[MAXGECOS+1];

	char away[MAXAWAY+1];

	u_ratelimit_t limit;

	u_conn *link; /* never null, except when shutting down */
	u_oper *oper; /* local opers only */
	u_server *sv; /* never null */
};

#define IS_LOCAL_USER(u) ((u->flags & USER_IS_LOCAL) != 0)

extern mowgli_patricia_t *users_by_nick;
extern mowgli_patricia_t *users_by_uid;

extern u_mode_info umode_infotab[128];
extern u_mode_ctx umodes;
extern uint umode_default;

extern u_user *u_user_create_local(u_conn *conn);
extern u_user *u_user_create_remote(u_server*, char *uid);
extern void u_user_destroy(u_user*);

extern void u_user_try_register(u_user*);

extern u_user *u_user_by_nick(char*);
extern u_user *u_user_by_uid(char*);

static inline u_user *u_user_by_ref(u_conn *conn, char *ref)
{
	if (!ref) return NULL;
	return (conn && conn->ctx == CTX_SERVER && isdigit(*ref)) ?
	        u_user_by_uid(ref) : u_user_by_nick(ref);
}

extern char *u_user_modes(u_user*);

extern void u_user_set_nick(u_user*, char*, uint);

extern void u_user_vnum(u_user*, int, va_list);
extern int u_user_num(u_user*, int num, ...);

extern void u_user_send_isupport(u_user*);
extern void u_user_send_motd(u_user*);

extern void u_user_welcome(u_user*);

extern void u_user_make_euid(u_user*, char *buf); /* sizeof(buf) > 512 */

extern int init_user(void);

#endif

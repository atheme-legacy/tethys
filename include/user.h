/* ircd-micro, user.h -- user management
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

/* all users */
#define USER_MASK_UMODE        0x000000ff
#define UMODE_OPER             0x00000001
#define UMODE_INVISIBLE        0x00000002
#define UMODE_WALLOPS          0x00000004
#define UMODE_CLOAKED          0x00000008

#define USER_MASK_FLAGS        0x0000ff00
#define USER_IS_LOCAL          0x00000100

/* local */
#define USER_MASK_CAP          0x00ff0000
#define CAP_MULTI_PREFIX       0x00010000
#define CAP_AWAY_NOTIFY        0x00020000

#define USER_MASK_STATE        0xff000000
#define USER_REGISTERING       0x01000000
#define USER_CAP_NEGOTIATION   0x02000000
#define USER_CONNECTED         0x03000000
#define USER_DISCONNECTED      0x04000000
#define USER_NO_STATE          0xff000000  /* remote users */

typedef struct u_umode_info u_umode_info;
typedef struct u_user u_user;
typedef struct u_user_local u_user_local;
typedef struct u_user_remote u_user_remote;

struct u_umode_info {
	char ch;
	uint mask;
	void (*cb)(u_umode_info*, u_user*, int on);
};

struct u_user {
	char uid[10];
	uint flags;
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
};

struct u_user_local {
	u_user user;
	u_conn *conn;
	u_oper *oper;
};

struct u_user_remote {
	u_user user;
	u_server *server;
};

#define USER(U) ((u_user*)(U))
#define USER_LOCAL(U) ((u_user_local*)(U))
#define USER_REMOTE(U) ((u_user_remote*)(U))

#define IS_LOCAL_USER(u) ((USER(u)->flags & USER_IS_LOCAL) != 0)

extern mowgli_patricia_t *users_by_nick;
extern mowgli_patricia_t *users_by_uid;

extern u_umode_info *umodes;
extern uint umode_default;

extern u_user_local *ircduser;

extern void u_user_m_start(u_user*);
extern void u_user_m_end(u_user*);
extern void u_user_mode(u_user*, char ch, int on);

extern u_user_local *u_user_local_create(char *ip, char *host);
extern void u_user_make_ureg(u_conn*);
extern u_user_remote *u_user_new_remote(u_server*, char *uid);
extern void u_user_unlink(u_user*);

extern u_conn *u_user_conn(u_user*);
extern u_server *u_user_server(u_user*);

extern u_user *u_user_by_nick(char*);
extern u_user *u_user_by_uid(char*);

extern void u_user_set_nick(u_user*, char*, uint);
extern uint u_user_state(u_user*, uint);

extern void u_user_vnum(u_user*, int, va_list);
extern int u_user_num(u_user*, int num, ...);

extern void u_user_send_isupport(u_user_local*);
extern void u_user_welcome(u_user_local*);
extern void u_user_send_motd(u_user_local*);

extern void u_user_make_euid(u_user*, char *buf); /* sizeof(buf) > 512 */

extern int init_user(void);

#endif

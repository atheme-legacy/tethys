#ifndef __INC_USER_H__
#define __INC_USER_H__

/* There's no real rationale behind these values other
   than "they looked good" */
#define MAXNICKLEN 20
#define MAXIDENT 16
#define MAXHOST 120
#define MAXGECOS 40

#define USER_MASK_UMODE        0x00ff
#define USER_MASK_FLAGS        0x0f00
#define USER_MASK_STATE        0xf000

#define UMODE_OPER             0x0001
#define UMODE_INVISIBLE        0x0002
#define UMODE_WALLOPS          0x0004
#define UMODE_CLOAKED          0x0008

#define USER_IS_LOCAL          0x0100

#define USER_REGISTERING       0x1000
#define USER_CAP_NEGOTIATION   0x2000
#define USER_CONNECTED         0x3000
#define USER_DISCONNECTED      0x4000

#define CAP_MULTI_PREFIX       0x0001
#define CAP_AWAY_NOTIFY        0x0002

struct u_umode_info {
	char ch;
	unsigned mask;
};

struct u_user {
	char uid[10];
	char nick[MAXNICKLEN+1];
	char ident[MAXIDENT+1];
	char host[MAXHOST+1];
	char gecos[MAXGECOS+1];

	unsigned mode;
};

struct u_user_local {
	struct u_user user;
	struct u_conn *conn;

	unsigned flags;
};

struct u_user_remote {
	struct u_user user;
	struct u_server *server;
};

#define USER(U) ((struct u_user*)(U))

extern struct u_umode_info *umodes;
extern unsigned umode_default;

extern void u_user_local_init(); /* u_user_local*, u_conn* */
extern void u_user_remote_init(); /* u_user_remote*, u_server* */

extern int init_user();

#endif

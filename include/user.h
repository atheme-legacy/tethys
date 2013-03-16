#ifndef __INC_USER_H__
#define __INC_USER_H__

/* There's no real rationale behind these values other
   than "they looked good" */
#define MAXNICKLEN 20
#define MAXIDENT 16
#define MAXHOST 120
#define MAXGECOS 40

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

	unsigned flags;
};

struct u_user_local {
	struct u_user user;
	struct u_conn *conn;
};

struct u_user_remote {
	struct u_user user;
	struct u_server *server;
};

#define USER(U) ((struct u_user*)(U))

extern struct u_umode_info *umodes;
extern unsigned umode_default;

extern void u_user_make_ureg(); /* u_conn* */

extern struct u_user *u_user_by_nick(); /* char* */
extern struct u_user *u_user_by_uid(); /* char* */

extern unsigned u_user_state(); /* struct u_user*, unsigned */

extern void u_user_num(A3(struct u_user *u, int num, ...));

extern void u_user_send_motd(); /* u_user_local* */

extern int init_user();

#endif

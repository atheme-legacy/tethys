#include "ircd.h"

static struct u_umode_info __umodes[32] = {
	{ 'o', UMODE_OPER },
	{ 'i', UMODE_INVISIBLE },
	{ 'w', UMODE_WALLOPS },
	{ 'x', UMODE_CLOAKED },
	{ 0, 0 }
};

struct u_umode_info *umodes = __umodes;
unsigned umode_default = UMODE_INVISIBLE;

/* TODO: initialize these */
struct u_hash users_by_nick;
struct u_hash users_by_uid;

char *id_map = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
int id_modulus = 36; /* just strlen(uid_map) */
int id_digits[6] = {0, 0, 0, 0, 0, 0};
char id_buf[7] = {0, 0, 0, 0, 0, 0, 0};

char *id_next()
{
	int i;
	for (i=0; i<6; i++)
		id_buf[i] = id_map[id_digits[i]];
	for (i=6; i-->0;) {
		id_digits[i] ++;
		if (id_digits[i] < id_modulus)
			break;
		id_digits[i] -= id_modulus;
	}
	id_buf[6] = '\0';
	return id_buf;
}

void u_user_init(user)
struct u_user *user;
{
	user->id[0] = '\0';
	user->nick[0] = '\0';
	user->ident[0] = '\0';
	user->host[0] = '\0';
	user->gecos[0] = '\0';

	user->mode = umode_default | USER_REGISTERING;
}

void u_user_local_init(user, conn)
struct u_user_local *user;
struct u_conn *conn;
{
	u_user_init(USER(user));

	user->user.mode |= USER_IS_LOCAL;
	user->conn = conn;
	user->flags = 0;
}

void u_user_server_init(user, server)
struct u_user_remote *user;
struct u_server *server;
{
	u_user_init(USER(user));

	user->server = server;
}

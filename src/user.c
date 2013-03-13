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

struct u_trie *users_by_nick;
struct u_trie *users_by_uid;

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
	user->uid[0] = '\0';
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

struct u_user *u_user_by_nick(nick)
char *nick;
{
	return u_trie_get(users_by_nick, nick);
}

struct u_user *u_user_by_uid(uid)
char *uid;
{
	return u_trie_get(users_by_uid, uid);
}

int init_user()
{
	users_by_nick = u_trie_new(rfc1459_canonize);
	users_by_uid = u_trie_new(ascii_canonize);

	return (users_by_nick && users_by_uid) ? 0 : -1;
}

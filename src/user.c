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

/* used to simplify user_local_event */
void user_local_die(conn, msg)
struct u_conn *conn;
char *msg;
{
	struct u_user *u = conn->priv;
	u_conn_out_clear(conn);
	if (conn->ctx == CTX_USER) {
		/* TODO */
		u_conn_f(conn, ":%s!%s@%s QUIT :Error: %s",
		         u->nick, u->ident, u->host, msg);
	} else {
		u_conn_f(conn, "ERROR :%s", msg);
	}
}

void user_local_event(conn, event)
struct u_conn *conn;
{
	switch (event) {
	case EV_END_OF_STREAM:
		user_local_die(conn, "end of stream");
		break;
	case EV_RECV_ERROR:
		user_local_die(conn, "read error");
		break;
	case EV_SEND_ERROR:
		user_local_die(conn, "send error");
		break;
	case EV_SENDQ_FULL:
		user_local_die(conn, "sendq full");
		break;
	case EV_RECVQ_FULL:
		user_local_die(conn, "recvq full");
		break;
	default:
		user_local_die(conn, "unknown error");
		break;

	case EV_DESTROYING:
		free(conn->priv);
	}
}

void u_user_make_ureg(conn)
struct u_conn *conn;
{
	struct u_user_local *u;

	if (conn->ctx != CTX_UNREG && conn->ctx != CTX_UREG)
		return;

	conn->ctx = CTX_UREG;

	if (conn->priv != NULL)
		return;

	/* TODO: heap! */
	conn->priv = u = malloc(sizeof(*u));
	memset(u, 0, sizeof(*u));

	u_strlcpy(USER(u)->host, conn->ip, MAXHOST+1);

	USER(u)->flags = umode_default | USER_IS_LOCAL;
	u_user_state(USER(u), USER_REGISTERING);
	u->conn = conn;

	conn->event = user_local_event;
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

unsigned u_user_state(u, state)
struct u_user *u;
unsigned state;
{
	if (state & USER_MASK_STATE) {
		u->flags &= ~USER_MASK_STATE;
		u->flags |= (state & USER_MASK_STATE);
	}

	u_debug("USER FLAGS: [%p] %08xx\n", u, u->flags);

	return u->flags & USER_MASK_STATE;
}

int init_user()
{
	users_by_nick = u_trie_new(rfc1459_canonize);
	users_by_uid = u_trie_new(ascii_canonize);

	return (users_by_nick && users_by_uid) ? 0 : -1;
}

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
	struct u_user_local *ul;

	if (conn->ctx != CTX_UNREG && conn->ctx != CTX_UREG)
		return;

	conn->ctx = CTX_UREG;

	if (conn->priv != NULL)
		return;

	/* TODO: heap! */
	conn->priv = ul = malloc(sizeof(*ul));
	memset(ul, 0, sizeof(*ul));

	u_strlcpy(USER(ul)->host, conn->ip, MAXHOST+1);

	USER(ul)->flags = umode_default | USER_IS_LOCAL;
	u_user_state(USER(ul), USER_REGISTERING);
	ul->conn = conn;

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

	return u->flags & USER_MASK_STATE;
}

#ifdef STDARG
void u_user_num(struct u_user *u, int num, ...)
#else
void u_user_num(u, num, va_alist)
struct u_user *u;
int num;
va_dcl
#endif
{
	char buf[4096];
	char *fmt, *nick;
	struct u_conn *conn;
	va_list va; 

	if (u->flags & USER_IS_LOCAL) {
		conn = ((struct u_user_local*)u)->conn;
	} else {
		u_log(LG_SEVERE, "Can't send numerics to remote users yet!");
		return;
	}

	fmt = u_numeric_fmt[num];

	if (fmt == NULL) {
		u_log(LG_SEVERE, "Attempted to use NULL numeric %d", num);
		return;
	}

	va_start(va, num);
	vsprintf(buf, fmt, va);
	va_end(va);

	nick = u->nick;
	if (!*nick)
		nick = "*";

	u_conn_f(conn, ":%s %03d %s %s", me.name, num, nick, buf);
}

void u_user_send_motd(ul)
struct u_user_local *ul;
{
	struct u_list *n;
	struct u_user *u = USER(ul);

	if (u_list_is_empty(&my_motd)) {
		u_user_num(u, ERR_NOMOTD);
		return;
	}

	u_user_num(u, RPL_MOTDSTART, me.name);
	U_LIST_EACH(n, &my_motd)
		u_user_num(u, RPL_MOTD, n->data);
	u_user_num(u, RPL_ENDOFMOTD);
}

int init_user()
{
	users_by_nick = u_trie_new(rfc1459_canonize);
	users_by_uid = u_trie_new(ascii_canonize);

	return (users_by_nick && users_by_uid) ? 0 : -1;
}

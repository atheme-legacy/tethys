#include "ircd.h"

static void m_pass(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	if (conn->pass != NULL)
		free(conn->pass);
	conn->pass = (char*)malloc(strlen(msg->argv[0])+1);
	strcpy(conn->pass, msg->argv[0]);
}

static void try_reg(conn)
struct u_conn *conn;
{
	struct u_user *u = conn->priv;

	if (u_user_state(u, 0) != USER_REGISTERING || !u->nick[0]
			|| !u->ident[0] || !u->gecos[0])
		return;

	u_conn_f(conn, "Welcome!");
	u_conn_f(conn, "nick=%s ident=%s gecos=%s", u->nick, u->ident, u->gecos);

	conn->ctx = CTX_USER;
}

static void m_nick(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	struct u_user_local *u;
	char buf[MAXNICKLEN+1];

	u_user_make_ureg(conn);
	u = conn->priv;

	u_strlcpy(buf, msg->argv[0], MAXNICKLEN+1);

	if (!is_valid_nick(buf)) {
		u_conn_f(conn, "invalid nickname");
		return;
	}

	if (u_user_by_nick(buf)) {
		u_conn_f(conn, "nickname is in use");
		return;
	}

	strcpy(USER(u)->nick, buf);
	try_reg(conn);
}

static void m_user(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	struct u_user_local *u;
	char buf[MAXIDENT+1];

	u_user_make_ureg(conn);
	u = conn->priv;

	u_strlcpy(buf, msg->argv[0], MAXIDENT+1);
	if (!is_valid_ident(buf)) {
		u_conn_f(conn, "invalid ident");
		return;
	}
	strcpy(USER(u)->ident, buf);
	u_strlcpy(USER(u)->gecos, msg->argv[3], MAXGECOS+1);

	try_reg(conn);
}

static void m_cap(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	struct u_user_local *u;

	u_user_make_ureg(conn);
	u = conn->priv;
	u_user_state(USER(u), USER_CAP_NEGOTIATION);

	ascii_canonize(msg->argv[0]);
	if (!strcmp(msg->argv[0], "ls")) {
		u_conn_f(conn, "list capabs");
	} else if (!strcmp(msg->argv[0], "req")) {
		u_conn_f(conn, "cap ack");
	} else if (!strcmp(msg->argv[0], "end")) {
		u_conn_f(conn, "cap end");
		u_user_state(USER(u), USER_REGISTERING);
	}

	try_reg(conn);
}

struct u_cmd c_ureg[] = {
	{ "PASS", CTX_UNREG,  m_pass, 1 },
	{ "PASS", CTX_UREG,   m_pass, 1 },
	{ "NICK", CTX_UNREG,  m_nick, 1 },
	{ "NICK", CTX_UREG,   m_nick, 1 },
	{ "USER", CTX_UNREG,  m_user, 4 },
	{ "USER", CTX_UREG,   m_user, 4 },
	{ "CAP",  CTX_UNREG,  m_cap,  1 },
	{ "CAP",  CTX_UREG,   m_cap,  1 },
	{ "" }
};

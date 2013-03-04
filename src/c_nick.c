#include "ircd.h"

void cc_nick(conn, msg)
struct u_connection *conn;
struct u_msg *msg;
{
	printf("NICK %s\n", msg->argv[0]);
}

void cu_nick(user, msg)
struct u_user *user;
struct u_msg *msg;
{
	printf("CHANGE NICK %s -> %s\n", user->nick, msg->argv[0]);
}

struct u_cmd c_nick[] = {
	{ "NICK", cc_nick,1, cu_nick,1, NULL,0 },
	{ "" }
};

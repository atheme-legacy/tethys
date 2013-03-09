#include "ircd.h"

void cc_user(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	u_debug("REGISTER USER %s %s\n", msg->argv[0], msg->argv[3]);
}

void cu_user(user, msg)
struct u_user *user;
struct u_msg *msg;
{
	u_debug("REREGISTER ATTEMPT by %s\n");
}

struct u_cmd c_user[] = {
	{ "USER", cc_user,4, cu_user,0, NULL,0 },
	{ "" }
};

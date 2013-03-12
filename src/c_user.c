#include "ircd.h"

void cc_user(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	u_debug("REGISTER USER %s %s\n", msg->argv[0], msg->argv[3]);
}

struct u_cmd c_user[] = {
	{ "USER", CTX_UNREG, cc_user, 4 },
	{ "" }
};

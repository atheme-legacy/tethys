#include "ircd.h"

void cc_nick(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	u_debug("NICK %s\n", msg->argv[0]);
}

struct u_cmd c_nick[] = {
	{ "NICK", CTX_UNREG,    cc_nick, 1 },
	{ "" }
};

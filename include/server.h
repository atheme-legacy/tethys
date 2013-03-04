#ifndef __INC_SERVER_H__
#define __INC_SERVER_H__

#define MAXSERVNAME 40
#define MAXSERVDESC 80

struct u_server {
	struct u_conn *conn;
	char sid[4];
	char name[MAXSERVNAME+1];
	char desc[MAXSERVDESC+1];
};

struct u_server me;

#endif

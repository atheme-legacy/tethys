#include "ircd.h"

char *ws_skip(s)
char *s;
{
	while (*s && isspace(*s))
		s++;
	return s;
}

char *ws_cut(s)
char *s;
{
	while (*s && !isspace(*s))
		s++;
	if (!*s) return s;
	*s++ = '\0';
	return ws_skip(s);
}

int u_msg_parse(msg, s)
struct u_msg *msg;
char *s;
{
	int i;
	char *p;

	s = ws_skip(s);
	if (!*s) return -1;

	if (*s == ':') {
		msg->source = ++s;
		s = ws_cut(s);
		if (!*s) return -1;
	} else {
		msg->source = NULL;
	}

	msg->command = s;
	s = ws_cut(s);

	for (msg->argc=0; msg->argc<U_MSG_MAXARGS && *s;) {
		if (*s == ':') {
			msg->argv[msg->argc++] = ++s;
			break;
		}

		msg->argv[msg->argc++] = s;
		s = ws_cut(s);
	}

	return 0;
}

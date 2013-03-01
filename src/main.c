#include "ircd.h"

#define OUTBUFSIZE 4096

struct cmd_invocation {
	char *invoker;
	char *command;
	char *text;
	char *replyto;
	int use_privmsg;
};

struct command {
	/* accepts a cmd_invocation* as its arg */
	void (*cb)();
};

char *my_nick = "microbot";
char *my_chan = "#testing";
char cmd_prefix = '-';

struct u_io base_io;
struct u_io_fd *base_iofd;
struct u_linebuf linebuf;

struct u_hash commands;

char outbuf[OUTBUFSIZE];
int outbuflen = 0;

void do_send();
void do_recv();

void outbuf_put(buf, len)
char *buf;
int len;
{
	if (len + outbuflen > OUTBUFSIZE)
		len = OUTBUFSIZE - outbuflen;
	if (len == 0)
		return;

	memcpy(outbuf + outbuflen, buf, len);
	outbuflen += len;

	base_iofd->send = do_send;
}

void outbuf_send()
{
	int sz;

	if (outbuflen == 0)
		return;

	sz = send(base_iofd->fd, outbuf, outbuflen, 0);
	u_memmove(outbuf, outbuf + sz, OUTBUFSIZE - sz);
	outbuflen -= sz;

	if (outbuflen == 0)
		base_iofd->send = NULL;
}

int outbuf_empty()
{
	return outbuflen == 0;
}

int do_connect(host, port)
char *host;
short port;
{
	struct hostent *he;
	struct sockaddr_in sa;
	int sockfd;

	he = gethostbyname(host);
	if (he == NULL || he->h_length < 1)
		return -1;

	memcpy(&sa.sin_addr, he->h_addr_list[0], sizeof(sa.sin_addr));
	sa.sin_port = htons(port);
	sa.sin_family = AF_INET;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		return -1;

	if (connect(sockfd, &sa, sizeof(sa)) < 0) {
		close(sockfd);
		return -1;
	}

	base_iofd = u_io_add_fd(&base_io, sockfd);
}

void net_writef(fmt, va_alist)
char *fmt;
va_dcl
{
	char buf[1024];
	int sz;
	va_list va;

	va_start(va);
	sz = vsprintf(buf, fmt, va);
	va_end(va);

	outbuf_put(buf, sz);
	outbuf_put("\r\n", 2);
}

void cmd_success(cmd, fmt, va_alist)
struct cmd_invocation *cmd;
char *fmt;
va_dcl
{
	char buf[1024];
	int sz;
	va_list va;

	va_start(va);
	sz = vsprintf(buf, fmt, va);
	va_end(va);

	net_writef("%s %s :%s%s%s",
		cmd->use_privmsg? "PRIVMSG" : "NOTICE",
		cmd->replyto,
		cmd->use_privmsg? cmd->invoker : "",
		cmd->use_privmsg? ": " : "",
		buf);
}

void cmd_failure(cmd, fmt, va_alist)
struct cmd_invocation *cmd;
char *fmt;
va_dcl
{
	char buf[1024];
	int sz;
	va_list va;

	if (cmd->use_privmsg)
		return;

	va_start(va);
	sz = vsprintf(buf, fmt, va);
	va_end(va);

	net_writef("NOTICE %s :%s", cmd->replyto, buf);
}

int extract_cmd(cmd, msg)
struct cmd_invocation *cmd;
struct u_msg *msg;
{
	char *s;

	if (msg->argc != 2)
		return; /* should never happen */

	cmd->invoker = msg->source;
	s = (char*)strchr(cmd->invoker, '!');
	if (s != NULL)
		*s = '\0';

	if (!strcasecmp(msg->argv[0], my_nick)) {
		cmd->replyto = cmd->invoker;
		s = msg->argv[1];
		if (*s == cmd_prefix)
			s++;
		goto iscmd;
	}

	if (msg->argv[1][0] == cmd_prefix) {
		cmd->replyto = msg->argv[0];
		cmd->use_privmsg = 1;
		s = msg->argv[1] + 1;
		goto iscmd;
	}

	if (!strncasecmp(msg->argv[1], my_nick, strlen(my_nick))) {
		cmd->replyto = msg->argv[0];
		cmd->use_privmsg = 1;
		s = msg->argv[1];
		while (*s && !isspace(*s)) s++;
		while (*s && isspace(*s)) s++;
		goto iscmd;
	}

	return -1;

 iscmd:
	cmd->command = s;
	while (*s && !isspace(*s)) s++;
	*s = '\0';
	while (*s && isspace(*s)) s++;
	cmd->text = s;

	return 0;
}

void dispatch_cmd(cmd)
struct cmd_invocation *cmd;
{
	char *s;
	struct command *cmdent;

	for (s=cmd->command; *s; s++)
		*s = toupper(*s);

	printf("%s used %s: %s\n", cmd->invoker, cmd->command, cmd->text);

	cmdent = u_hash_get(&commands, cmd->command);

	if (cmdent == NULL) {
		cmd_failure(cmd, "No command %s", cmd->command);
		return;
	}

	cmdent->cb(cmd);
}

void add_cmd(name, cmd)
char *name;
void (*cmd)();
{
	struct command *cmdent;

	cmdent = u_hash_get(&commands, name);
	if (cmdent != NULL)
		return;

	cmdent = (void*)malloc(sizeof(*cmdent));

	cmdent->cb = cmd;

	u_hash_set(&commands, name, cmdent);
}

void do_dispatch(msg)
struct u_msg *msg;
{
	struct cmd_invocation cmd;
	if (!strcasecmp(msg->command, "PRIVMSG")) {
		if (extract_cmd(&cmd, msg) < 0)
			return;
		dispatch_cmd(&cmd);
	} else if (!strcasecmp(msg->command, "PING")) {
		net_writef("PONG :%s", msg->argv[msg->argc-1]);
	} else if (!strcasecmp(msg->command, "001")) {
		net_writef("JOIN %s", my_chan);
	}
}

void do_recv(iofd)
struct u_io_fd *iofd;
{
	char buf[1024];
	struct u_msg msg;
	int sz;

	sz = recv(iofd->fd, buf, 1024, 0);
	if (sz == 0) {
		close(iofd->fd);
		iofd->recv = NULL;
	}
	u_linebuf_data(&linebuf, buf, sz);


	for (;;) {
		sz = u_linebuf_line(&linebuf, buf, 1024);
		if (sz <= 0)
			break;
		buf[sz] = '\0';
		printf(" -> %s\n", buf);
		u_msg_parse(&msg, buf);
		do_dispatch(&msg);
	}
}

void do_send(iofd)
struct u_io_fd *iofd;
{
	outbuf_send();
}

void cmd_ping(cmd)
struct cmd_invocation *cmd;
{
	cmd_success(cmd, "pong!");
}

void cmd_flip(cmd)
struct cmd_invocation *cmd;
{
	if (rand() % 2) {
		cmd_success(cmd, "heads!");
	} else {
		cmd_success(cmd, "tails!");
	}
}

int main(argc, argv)
int argc;
char *argv[];
{
	if (argc != 3) {
		printf("Usage: %s HOST PORT\n", argv[0]);
		return 1;
	}

	u_hash_init(&commands);
	u_io_init(&base_io);
	u_linebuf_init(&linebuf);

	if (do_connect(argv[1], atoi(argv[2])) < 0) {
		printf("Connect failed\n");
		return 1;
	}

	base_iofd->recv = do_recv;
	base_iofd->send = NULL;

	add_cmd("PING", cmd_ping);
	add_cmd("FLIP", cmd_flip);

	net_writef("NICK %s", my_nick);
	net_writef("USER %s * 8 :microbot", my_nick);

	printf("Starting...\n");

	u_io_poll(&base_io);

	return 0;
}

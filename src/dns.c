#include "ircd.h"

/* the only types we care about */
#define DNS_T_A        1
#define DNS_T_NS       2
#define DNS_T_CNAME    5
#define DNS_T_PTR     12
#define DNS_T_TXT     16

/* the only class that matters */
#define DNS_CLASS_IN   1

#define DNSBUFSIZE 512

struct dns_req {
	unsigned short id;
	struct u_io_timer *timeout;
	void (*cb)();
	void *priv;
};

struct u_list reqs;
int dnsfd;
struct u_io_fd *sock = NULL;

unsigned char msg[DNSBUFSIZE];
int msghead, msgtail;

void msg_reset()
{
	msghead = msgtail = 0;
}

unsigned short msg_get16()
{
	if (msgtail - msghead < 2)
		return 0; /* XXX */
	msghead += 2;
	return (msg[msghead-2]<<8) | msg[msghead-1];
}

void msg_gets(dst)
char *dst;
{
	unsigned char len = msg[msghead++];
	if (msgtail - msghead < len)
		len = msgtail - msghead; /* XXX */
	for (; len>0; len--)
		*dst++ = msg[msghead++];
}

void msg_put16(sh)
short sh;
{
	if (msgtail + 2 > DNSBUFSIZE)
		return; /* XXX */
	msg[msgtail++] = (sh >> 8) & 0xff;
	msg[msgtail++] = sh & 0xff;
}

void msg_puts(str, n)
char *str;
int n;
{
	if (n < 0)
		n = strlen(str);
	if (n > 255)
		return; /* XXX */
	if (msgtail - msghead + 1 + n > DNSBUFSIZE)
		n = DNSBUFSIZE - (msgtail - msghead) - 1; /* XXX */
	msg[msgtail++] = 0xff & ((unsigned)n);
	for (; n>0; n--)
		msg[msgtail++] = *str++;
}

void dns_recv(iofd)
struct u_io_fd *iofd;
{
	struct sockaddr addr;
	int addrlen;

	msghead = 0;
	msgtail = recvfrom(dnsfd, msg, BUFSIZE, 0, &addr, &addrlen);
	if (msgtail < 0) {
		msgtail = 0;
		u_log(LG_ERROR, "dns_recv() recvfrom error");
		return;
	}
}

void u_dns_use_io(io)
struct u_io *io;
{
	if (sock != NULL) {
		sock->recv = NULL;
		sock->send = NULL;
		sock = NULL;
		/* io will delete it for us */
	}

	sock = u_io_add_fd(io, dnsfd);
	sock->recv = dns_recv;
	sock->send = NULL;
}

void u_dns(name, cb, priv)
char *name;
void (*cb)();
void *priv;
{
	msg_reset();
	/* TODO: make message */
}

void u_rdns(name, cb, priv)
char *name;
void (*cb)();
void *priv;
{
	struct in_addr in;

	if (inet_aton(name, &in)) {
		/* XXX: we should not call the callback from within the same
		   stack context as u_rdns! we should instead set a timer
		   for 0 seconds and call the callback from there so as to
		   prevent strange bugs with cb modifying priv in a way that
		   the caller does not expect */
		cb(DNS_INVALID, NULL, priv);
		return;
	}

	msg_reset();
	/* TODO: make message */
}

int init_dns()
{
	dnsfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (dnsfd < 0)
		return -1;

	/* TODO: bind? */

	u_list_init(&reqs);
	return 0;
}

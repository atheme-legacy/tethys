#include "ircd.h"

#define DNS_OP_QUERY       0x0000
#define DNS_OP_IQUERY      0x0800
#define DNS_OP_STATUS      0x1000

#define DNS_FLAG_AA        0x0400
#define DNS_FLAG_TC        0x0200
#define DNS_FLAG_RD        0x0100
#define DNS_FLAG_RA        0x0080

#define DNS_MASK_Z         0x0070
#define DNS_MASK_RCODE     0x000f

#define DNS_RCODE_OK       0x0000
#define DNS_RCODE_FMTERR   0x0001
#define DNS_RCODE_INTLERR  0x0002
#define DNS_RCODE_NAMEERR  0x0003
#define DNS_RCODE_NOTIMPL  0x0004
#define DNS_RCODE_REFUSED  0x0005

/* the only types we care about */
#define DNS_TYPE_A              1
#define DNS_TYPE_NS             2
#define DNS_TYPE_CNAME          5
#define DNS_TYPE_PTR           12
#define DNS_TYPE_TXT           16

/* the only class that matters */
#define DNS_CLASS_IN            1

#define DNSBUFSIZE            512

struct dns_hdr {
	unsigned short id;
	unsigned short flags;
	unsigned short qdcount;
	unsigned short ancount;
	unsigned short nscount;
	unsigned short arcount;
};

struct dns_req {
	unsigned short id;
	struct u_io_timer *timeout;
	void (*cb)();
	void *priv;
};

struct dns_rr {
	char name[DNSBUFSIZE];
	unsigned short type;
	unsigned short class;
	unsigned int ttl;
	unsigned short rdlength;
	char rdata[DNSBUFSIZE];
};

struct u_list reqs;
int dnsfd;
struct u_io_fd *sock = NULL;

unsigned char msg[DNSBUFSIZE];
int msghead, msgtail;

unsigned short make_id()
{
	/* XXX */
	return rand();
}

struct u_list *find_req(id)
unsigned short id;
{
	struct u_list *n;
	struct dns_req *req;
	U_LIST_EACH(n, &reqs) {
		req = n->data;
		if (req->id == id)
			return n;
	}
	return NULL;
}

void msg_reset()
{
	msghead = msgtail = 0;
}

void msg_dump()
{
	unsigned char *p = msg+msghead;
	int i, len = msgtail-msghead;
	for (i=0; i<len; i++) {
		if (i%16 == 0) {
			if (i!=0)
				putchar('\n');
			putchar('\t');
		}
		printf("%02x", p[i]);
		if (i%16 != 15)
			putchar(' ');
	}
	putchar('\n');
}

unsigned short msg_get16_real(p)
unsigned char **p;
{
	unsigned char *s = *p;
	(*p) += 2;
	return (s[0]<<8) | s[1];
}

unsigned short msg_get16()
{
	unsigned char *p = msg + msghead;
	msghead += 2;
	return msg_get16_real(&p);
}

unsigned int msg_get32()
{
	unsigned char *s;
	if (msgtail - msghead < 4)
		return 0; /* XXX */
	s = msg + msghead;
	msghead += 4;
	return (s[0]<<24) | (s[1]<<16) | (s[2]<<8) | s[3];
}

void msg_gethdr(hdr)
struct dns_hdr *hdr;
{
	hdr->id = msg_get16();
	hdr->flags = msg_get16();
	hdr->qdcount = msg_get16();
	hdr->ancount = msg_get16();
	hdr->nscount = msg_get16();
	hdr->arcount = msg_get16();
}

void get_name_real(s, p)
unsigned char *s, **p;
{
	unsigned char len, *q;

	if ((**p & 0xc0) != 0) {
		q = msg + (msg_get16_real(p) & 0x3fff);
		get_name_real(s, &q);
		return;
	}

	len = *(*p)++;
	if (len == 0) {
		*s = '\0';
		return;
	}

	q = s;
	for (; len>0; len--)
		*s++ = *(*p)++;
	*s++ = '.';

	get_name_real(s, p);
}

void get_name(s)
unsigned char *s;
{
	unsigned char *start, *p;

	start = msg + msghead;
	p = start;

	get_name_real(s, &p);

	msghead += p - start;
}

void msg_getrr(rr)
struct dns_rr *rr;
{
	char *s, *p;

	get_name(rr->name);
	puts(rr->name);

	rr->type = msg_get16();
	rr->class = msg_get16();
	rr->ttl = msg_get32();
	rr->rdlength = msg_get16();

	memcpy(rr->rdata, msg+msghead, rr->rdlength);
	msghead += rr->rdlength;
}

void skip_question()
{
	unsigned char n;
	char buf[DNSBUFSIZE];
	get_name(buf);
	msghead += 4; /* QTYPE, QCLASS */
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

void msg_puthdr(hdr)
struct dns_hdr *hdr;
{
	msg_put16(hdr->id);
	msg_put16(hdr->flags);
	msg_put16(hdr->qdcount);
	msg_put16(hdr->ancount);
	msg_put16(hdr->nscount);
	msg_put16(hdr->arcount);
}

void put_name(name)
char *name;
{
	char *s, *p;

	s = name;
	while (p) {
		p = strchr(s, '.');
		if (p != NULL)
			*p++ = '\0';
		msg_puts(s, strlen(s));
		s = p;
	}
}

void send_msg()
{
	struct sockaddr_in addr;
	struct sockaddr *sa = (struct sockaddr*)&addr;
	char *p;
	int len;

	u_log(LG_DEBUG, "dns: sending message");

	addr.sin_family = AF_INET;
	addr.sin_port = htons(53);
	inet_aton("8.8.8.8", &addr.sin_addr);

	p = msg+msghead;
	len = msgtail-msghead;
		
	u_log(LG_DEBUG, "sendto=%d",
	      sendto(dnsfd, p, len, 0, sa, sizeof(addr)));

	msg_dump();
} 

void dns_recv(iofd)
struct u_io_fd *iofd;
{
	struct sockaddr addr;
	int addrlen, err;
	struct dns_hdr hdr;
	struct u_list *reqn;
	struct dns_req *req;
	struct dns_rr rr;
	struct in_addr in;

	u_log(LG_DEBUG, "dns: receiving message");

	msghead = 0;
	msgtail = recvfrom(dnsfd, msg, DNSBUFSIZE, 0, &addr, &addrlen);
	if (msgtail < 0) {
		perror("dns_recv");
		msgtail = 0;
		u_log(LG_ERROR, "dns_recv() recvfrom error");
		return;
	}
	msg_dump();

	msg_gethdr(&hdr);
	reqn = find_req(hdr.id);
	if (req == NULL) {
		u_log(LG_ERROR, "dns_recv: no request with id %d!", (int)hdr.id);
		return;
	}
	req = reqn->data;

	if ((hdr.flags & DNS_MASK_RCODE) != DNS_RCODE_OK) {
		u_log(LG_ERROR, "dns_recv: response has RCODE 0x%x",
		      hdr.flags & DNS_MASK_RCODE);
		err = DNS_OTHER;
		if ((hdr.flags & DNS_MASK_RCODE) == DNS_RCODE_NAMEERR)
			err = DNS_NXDOMAIN;
		req->cb(err, NULL, req->priv);
		goto req_del;
	}

	for (; hdr.qdcount>0; hdr.qdcount--)
		skip_question();

	for (; hdr.ancount>0; hdr.ancount--) {
		msg_getrr(&rr);
		in.s_addr = *((unsigned int*)rr.rdata);
		u_log(LG_DEBUG, "rr: name=%s type=%d class=%d ttl=%d addr=%s",
		      rr.name, rr.type, rr.class, rr.ttl, inet_ntoa(in));
	}

req_del:
	u_list_del_n(reqn);
	free(req);
}

void u_dns_use_io(io)
struct u_io *io;
{
	if (sock != NULL) {
		u_log(LG_WARN, "u_dns_use_io called more than once!");
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
	struct dns_hdr hdr;
	struct dns_req *req;
	char buf[BUFSIZE];

	msg_reset();

	hdr.id = make_id();
	hdr.flags = DNS_OP_QUERY | DNS_FLAG_RD;
	hdr.qdcount = 1;
	hdr.ancount = 0;
	hdr.nscount = 0;
	hdr.arcount = 0;

	msg_puthdr(&hdr);

	sprintf(buf, "%s.", name);
	put_name(buf);
	msg_put16(DNS_TYPE_A);
	msg_put16(DNS_CLASS_IN);

	req = malloc(sizeof(*req));
	req->id = hdr.id;
	req->timeout = NULL; /* TODO */
	req->cb = cb;
	req->priv = priv;
	u_list_add(&reqs, req);

	send_msg();
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

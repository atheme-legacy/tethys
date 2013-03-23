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
#define DNSNAMESIZE           256

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
	char name[DNSNAMESIZE];
	int type; /* A = u_dns, PTR = u_rdns, for this */
	void (*cb)();
	void *priv;
};

struct dns_rr {
	char name[DNSNAMESIZE];
	unsigned short type;
	unsigned short class;
	unsigned int ttl;
	unsigned short rdlength;
	char rdata[DNSNAMESIZE];
};

struct u_list reqs;
int dnsfd;
struct u_io_fd *sock = NULL;

unsigned char msg[DNSBUFSIZE];
int msghead, msgtail;

/* not sure if there's a better way... */
unsigned int id_nfree_total = 65536;
unsigned char id_nfree[512];
unsigned int id_bitvec[2048];

/* brief break from style conventions here for compactness */
void id_bitvec_set(i) unsigned short i; {
	id_bitvec[i>>5] = id_bitvec[i>>5] | (1<<(i&0x1f)); }
void id_bitvec_reset(i) unsigned short i; {
	id_bitvec[i>>5] = id_bitvec[i>>5] & ~(1<<(i&0x1f)); }
unsigned char id_bitvec_get(i) unsigned short i; {
	return ((id_bitvec[i>>5]) & (1<<(i&0x1f))) != 0; }

unsigned char num_set_bits(n)
unsigned long n;
{
	unsigned char i, c = 0;
	/* http://graphics.stanford.edu/~seander/bithacks.html */
	n = n - ((n >> 1) & 0x55555555);
	n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
	return ((n + (n >> 4) & 0x0f0f0f0f) * 0x01010101) >> 24;
}

unsigned short id_alloc()
{
	unsigned short origoffs, offs, id;
	unsigned char t;

	if (id_nfree_total == 0) {
		u_log(LG_SEVERE, "Cannot allocate a new DNS request id!");
		return 0; /* XXX */
	}

	origoffs = offs = (unsigned)(rand()) % id_nfree_total;

	if (id_nfree_total == 65536) {
		/* likely, for quiet servers */
		id = offs;
	} else {
		id = 0;

		for (;; id += 128) {
			if (offs <= id_nfree[id>>7])
				break;
			offs -= id_nfree[id>>7];
		}

		for (;; id += 32) {
			t = 32 - num_set_bits(id_bitvec[id>>5]);
			if (offs <= t)
				break;
			offs -= t;
		}

		for (;; id ++) {
			if (!id_bitvec_get(id)) {
				if (offs == 0)
					break;
				offs--;
			}
		}
	}

	/*u_log(LG_FINE, "id_alloc: id=%d", id);*/

	if (id_bitvec_get(id)) {
		u_log(LG_SEVERE, "Generated duplicate id %d (offs=%d, nfree=%d)!",
		      id, origoffs, id_nfree_total);
		return;
	}

	id_bitvec_set(id);
	id_nfree_total--;
	id_nfree[id>>7]--;

	return id;
}

void id_free(id)
unsigned short id;
{
	if (id_bitvec_get(id) == 0)
		return;
	id_bitvec_reset(id);
	id_nfree_total++;
	id_nfree[id>>7]++;
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

void get_name_real(s, p, depth)
unsigned char *s, **p;
int depth;
{
	unsigned char len, *q;

	for (;;) {
		/* goto used to reduce indentation levels */
		if ((**p & 0xc0) != 0)
			goto ptr;

		/* first octet is length */
		len = *(*p)++;
		if (len == 0) {
			*s = '\0';
			return;
		}

		/* remaining octets are name */
		q = s;
		for (; len>0; len--)
			*s++ = *(*p)++;
		*s++ = '.';
	}

	return;

ptr:
	/* is a pointer. follow pointer */
	q = msg + (msg_get16_real(p) & 0x3fff);
	if (q > msg + msgtail || depth > 16) {
		/* XXX find a real way to complain */
		u_log(LG_ERROR, "Bad or malicious name in"
		      "DNS reply!");
		return;
	}
	get_name_real(s, &q, depth + 1);
}

void get_name(s)
unsigned char *s;
{
	unsigned char *start, *p;

	start = msg + msghead;
	p = start;

	get_name_real(s, &p, 1);

	msghead += p - start;
}

void msg_getrr(rr)
struct dns_rr *rr;
{
	char *s, *p;

	get_name(rr->name);

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
unsigned char *str;
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

void msg_putd(d)
unsigned char d;
{
	char buf[4];
	sprintf(buf, "%d", d);
	msg_puts(buf, strlen(buf));
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
	char buf[DNSNAMESIZE];

	u_strlcpy(buf, name, DNSNAMESIZE);
	p = buf;

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

void send_req(req)
struct dns_req *req;
{
	struct dns_hdr hdr;

	msg_reset();

	hdr.id = req->id;
	hdr.flags = DNS_OP_QUERY | DNS_FLAG_RD;
	hdr.qdcount = 1;
	hdr.ancount = 0;
	hdr.nscount = 0;
	hdr.arcount = 0;

	msg_puthdr(&hdr);

	put_name(req->name);
	msg_put16(req->type);
	msg_put16(DNS_CLASS_IN);

	send_msg();
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
	char *p, rdata[DNSBUFSIZE];

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

	u_log(LG_DEBUG, "qd=%d an=%d ns=%d ar=%d",
	      hdr.qdcount, hdr.ancount, hdr.nscount, hdr.arcount);

	for (; hdr.qdcount>0; hdr.qdcount--)
		skip_question();

	for (; hdr.ancount>0; hdr.ancount--) {
		msg_getrr(&rr);
		switch (rr.type) {
		case DNS_TYPE_A:
			in.s_addr = *((unsigned int*)rr.rdata);
			strcpy(rdata, (char*)inet_ntoa(in));
			break;
		case DNS_TYPE_CNAME:
		case DNS_TYPE_PTR:
			p = rr.rdata;
			get_name_real(rdata, &p);
			break;
		default:
			strcpy(rdata, "?");
			break;
		}
		u_log(LG_DEBUG, "rr: name=%s type=%d class=%d ttl=%d rdata=%s",
		      rr.name, rr.type, rr.class, rr.ttl, rdata);
	}

req_del:
	id_free(req->id);
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
	struct dns_req *req;

	req = malloc(sizeof(*req));
	req->id = id_alloc();
	req->timeout = NULL; /* TODO */
	req->type = DNS_TYPE_A;
	req->cb = cb;
	req->priv = priv;
	u_list_add(&reqs, req);

	sprintf(req->name, "%s.", name);

	send_req(req);
}

void u_rdns(name, cb, priv)
char *name;
void (*cb)();
void *priv;
{
	struct in_addr in;
	struct dns_req *req;
	unsigned char *p;

	if (!inet_aton(name, &in)) {
		/* XXX: we should not call the callback from within the same
		   stack context as u_rdns! we should instead set a timer
		   for 0 seconds and call the callback from there so as to
		   prevent strange bugs with cb modifying priv in a way that
		   the caller does not expect */
		cb(DNS_INVALID, NULL, priv);
		return;
	}

	p = (unsigned char*)&(in.s_addr);

	req = malloc(sizeof(*req));
	req->id = id_alloc();
	req->timeout = NULL; /* TODO */
	req->type = DNS_TYPE_PTR;
	req->cb = cb;
	req->priv = priv;
	u_list_add(&reqs, req);

	sprintf(req->name, "%u.%u.%u.%u.in-addr.arpa.",
	        p[3], p[2], p[1], p[0]);

	send_req(req);
}

int init_dns()
{
	int i;

	for (i=0; i<512; i++)
		id_nfree[i] = 128;
	for (i=0; i<2048; i++)
		id_bitvec[i] = 0;

	dnsfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (dnsfd < 0)
		return -1;

	/* TODO: bind? */

	u_list_init(&reqs);
	return 0;
}

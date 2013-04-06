#include "ircd.h"

#define DNS_CACHE_SIZE        100

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

typedef struct dns_hdr dns_hdr_t;
typedef struct dns_cb dns_cb_t;
typedef struct dns_req dns_req_t;
typedef struct dns_rr dns_rr_t;
typedef struct dns_cache_ent dns_cache_ent_t;

struct dns_hdr {
	ushort id;
	ushort flags;
	ushort qdcount;
	ushort ancount;
	ushort nscount;
	ushort arcount;
};

struct dns_cb {
	void (*cb)();
	void *priv;
	struct dns_cb *next;
};

struct dns_req {
	ushort id;
	u_list *n;
	u_io_timer *timeout;
	char name[DNSNAMESIZE];
	int type; /* A = u_dns, PTR = u_rdns, for this */
	dns_cb_t *cblist;
};

struct dns_rr {
	char name[DNSNAMESIZE];
	ushort type;
	ushort class;
	uint ttl;
	ushort rdlength;
	char rdata[DNSNAMESIZE];
};

struct dns_cache_ent {
	char req[DNSNAMESIZE];
	int status;
	char res[DNSNAMESIZE];
	ulong expires;
	u_list *n;
};

u_list reqs;

u_list cache_by_recent;
u_map *cache_by_name;
int cache_size = 0;

int dnsfd;
u_io *dnsio = NULL;
u_io_fd *dnssock = NULL;

uchar msg[DNSBUFSIZE];
int msghead, msgtail;

/* not sure if there's a better way... */
uint id_nfree_total = 65535;
uchar id_nfree[512];
uint id_bitvec[2048];

/* brief break from style conventions here for compactness */
void id_bitvec_set(i) ushort i; {
	id_bitvec[i>>5] = id_bitvec[i>>5] | (1<<(i&0x1f)); }
void id_bitvec_reset(i) ushort i; {
	id_bitvec[i>>5] = id_bitvec[i>>5] & ~(1<<(i&0x1f)); }
uchar id_bitvec_get(i) ushort i; {
	return ((id_bitvec[i>>5]) & (1<<(i&0x1f))) != 0; }

uchar num_set_bits(n) ulong n;
{
	/* http://graphics.stanford.edu/~seander/bithacks.html */
	n = n - ((n >> 1) & 0x55555555);
	n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
	return (((n + (n >> 4)) & 0x0f0f0f0f) * 0x01010101) >> 24;
}

ushort id_alloc()
{
	ushort origoffs, offs, id;
	uchar t;

	if (id_nfree_total == 0) {
		u_log(LG_SEVERE, "Cannot allocate a new DNS request id!");
		return 0; /* XXX */
	}

	origoffs = offs = ((uint)(rand()) % id_nfree_total) + 1;

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
		return 0;
	}

	id_bitvec_set(id);
	id_nfree_total--;
	id_nfree[id>>7]--;

	return id;
}

void id_free(id) ushort id;
{
	if (id_bitvec_get(id) == 0)
		return;
	id_bitvec_reset(id);
	id_nfree_total++;
	id_nfree[id>>7]++;
}

void cache_add(req, status, res, expires) char *req, *res; ulong expires;
{
	char buf[DNSNAMESIZE];
	dns_cache_ent_t *cached;

	u_strlcpy(buf, req, DNSNAMESIZE);
	req = buf;
	ascii_canonize(req);

	cached = u_map_get(cache_by_name, req);
	if (cached == NULL) {
		u_log(LG_FINE, "+++++ ALLOCATING CACHE ENT");
		cached = malloc(sizeof(*cached));
		if (cached == NULL)
			goto fail;
		u_map_set(cache_by_name, req, cached);
		cache_size++;
	} else {
		u_log(LG_FINE, "***** REUSING CACHE ENT");
		u_list_del_n(cached->n);
	}

	cached->n = u_list_add(&cache_by_recent, cached);
	if (cached->n == NULL) {
		u_map_del(cache_by_name, req);
		goto fail;
	}

	u_strlcpy(cached->req, req, DNSNAMESIZE);
	cached->status = status;
	u_strlcpy(cached->res, res, DNSNAMESIZE);
	cached->expires = expires;

	if (cache_size > DNS_CACHE_SIZE) {
		cached = u_list_del_n(cache_by_recent.next);
		if (cached != NULL) {
			u_log(LG_FINE, "DNS:CACHE: (full) dropping %s=>%s",
			      cached->req, cached->res);
			u_map_del(cache_by_name, cached->req);
			free(cached);
		}
		cache_size--;
	}

	u_log(LG_FINE, "DNS:CACHE: adding %s=>%s, status %d", req, res, status);

	return;

fail:
	u_log(LG_ERROR, "dns: Failed to cache %s=>%s status %d", req, res, status);
	return;
}

int cache_find(req, res) char *req, *res;
{
	char buf[DNSNAMESIZE];
	dns_cache_ent_t *cached;

	u_strlcpy(buf, req, DNSNAMESIZE);
	req = buf;
	ascii_canonize(req);

	*res = '\0';

	cached = u_map_get(cache_by_name, req);
	if (cached == NULL) {
		u_log(LG_FINE, "DNS:CACHE: cache miss on %s", req);
		return -1;
	}

	if (cached->expires < NOW.tv_sec) {
		u_log(LG_FINE, "DNS:CACHE: (expiry) dropping %s=>%s, status %d",
		      cached->req, cached->res, cached->status);
		u_list_del_n(cached->n);
		u_map_del(cache_by_name, cached->req);
		free(cached);
		cache_size--;
		return -1;
	}

	u_log(LG_FINE, "DNS:CACHE: cache hit %s=>%s, status %d",
	      cached->req, cached->res, cached->status);
	u_strlcpy(res, cached->res, DNSNAMESIZE);
	return cached->status;
}

dns_cb_t *req_cb_add(req, cb, priv)
dns_req_t *req; void (*cb)(); void *priv;
{
	dns_cb_t *cbn;

	if (cb == NULL) {
		u_log(LG_ERROR, "dns: refusing to add NULL callback");
		return NULL;
	}

	for (cbn=req->cblist; cbn; cbn=cbn->next) {
		if (cbn->cb == cb && cbn->priv == priv) {
			/* how do we properly deal with this? */
			u_log(LG_WARN, "dns: duplicate callbacks added to %s",
			      req->name);
			break;
		}
	}

	cbn = malloc(sizeof(*cbn));
	cbn->cb = cb;
	cbn->priv = priv;

	cbn->next = req->cblist;
	req->cblist = cbn;

	return cbn;
}

void req_cb_del(req, cb, priv)
dns_req_t *req; void (*cb)(); void *priv;
{
	dns_cb_t *cbn, *cbp;

	for (cbn=req->cblist,cbp=NULL; cbn; cbp=cbn,cbn=cbn->next) {
		if (cbn->cb != cb || cbn->priv != priv)
			continue;

		if (cbn == req->cblist)
			req->cblist = cbn->next;
		else
			cbp->next = cbn->next;

		free(cbn);
		return;
	}

	u_log(LG_WARN, "dns: couldn't find matching callback for %s", req->name);
}

void req_cb_del_all(req) dns_req_t *req;
{
	dns_cb_t *cbn, *cbt;

	for (cbn=req->cblist; cbn; ) {
		cbt = cbn->next;
		free(cbn);
		cbn = cbt;
	}

	req->cblist = NULL;
}

void req_cb_call(req, err, rdata)
dns_req_t *req; char *rdata;
{
	dns_cb_t *cbn = req->cblist;

	if (cbn == NULL)
		u_log(LG_WARN, "dns: no callbacks to call for %s!", req->name);

	for (; cbn; cbn=cbn->next)
		cbn->cb(err, rdata, cbn->priv);
}

dns_req_t *req_make(type, timeout) void (*timeout)();
{
	dns_req_t *req;
	req = malloc(sizeof(*req));
	req->id = id_alloc();
	req->n = u_list_add(&reqs, req);
	req->timeout = u_io_add_timer(dnsio, 1, 0, timeout, req);
	req->type = type;
	req->cblist = NULL;
	return req;
}

dns_req_t *req_find(id) ushort id;
{
	u_list *n;
	dns_req_t *req;
	U_LIST_EACH(n, &reqs) {
		req = n->data;
		if (req->id == id)
			return req;
	}
	return NULL;
}

dns_req_t *req_find_by_name(name) char *name;
{
	u_list *n;
	dns_req_t *req;
	U_LIST_EACH(n, &reqs) {
		req = n->data;
		if (!strcmp(req->name, name))
			return req;
	}
	return NULL;
}

void req_del(req) dns_req_t *req;
{
	id_free(req->id);
	u_io_del_timer(req->timeout);
	u_list_del_n(req->n);
	req_cb_del_all(req);
	free(req);
}

void msg_reset()
{
	msghead = msgtail = 0;
}

void msg_dump()
{
	/*
	uchar *p = msg+msghead;
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
	*/
}

ushort msg_get16_real(p) uchar **p;
{
	uchar *s = *p;
	(*p) += 2;
	return (s[0]<<8) | s[1];
}

ushort msg_get16()
{
	uchar *p = msg + msghead;
	msghead += 2;
	return msg_get16_real(&p);
}

uint msg_get32()
{
	uchar *s;
	if (msgtail - msghead < 4)
		return 0; /* XXX */
	s = msg + msghead;
	msghead += 4;
	return (s[0]<<24) | (s[1]<<16) | (s[2]<<8) | s[3];
}

void msg_gethdr(hdr) dns_hdr_t *hdr;
{
	hdr->id = msg_get16();
	hdr->flags = msg_get16();
	hdr->qdcount = msg_get16();
	hdr->ancount = msg_get16();
	hdr->nscount = msg_get16();
	hdr->arcount = msg_get16();
}

void get_name_real(s, p, depth) uchar *s, **p;
{
	uchar len, *q;

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

ptr:
	/* is a pointer. follow pointer */
	q = msg + (msg_get16_real(p) & 0x3fff);
	if (q > msg + msgtail || depth > 16) {
		/* XXX find a real way to complain */
		u_log(LG_ERROR, "Bad or malicious name in DNS reply!");
		*s = '\0';
	}
	get_name_real(s, &q, depth + 1);
}

void get_name(s) uchar *s;
{
	uchar *start, *p;

	start = msg + msghead;
	p = start;

	get_name_real(s, &p, 1);

	msghead += p - start;
}

void msg_getrr(rr) dns_rr_t *rr;
{
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
	char buf[DNSBUFSIZE];
	get_name(buf);
	msghead += 4; /* QTYPE, QCLASS */
}

void msg_put16(sh) short sh;
{
	if (msgtail + 2 > DNSBUFSIZE)
		return; /* XXX */
	msg[msgtail++] = (sh >> 8) & 0xff;
	msg[msgtail++] = sh & 0xff;
}

void msg_puts(str, n) uchar *str;
{
	if (n < 0)
		n = strlen((char*)str);
	if (n > 255)
		return; /* XXX */
	if (msgtail - msghead + 1 + n > DNSBUFSIZE)
		n = DNSBUFSIZE - (msgtail - msghead) - 1; /* XXX */
	msg[msgtail++] = 0xff & ((uint)n);
	for (; n>0; n--)
		msg[msgtail++] = *str++;
}

void msg_putd(d) uchar d;
{
	char buf[4];
	sprintf(buf, "%d", d);
	msg_puts(buf, strlen(buf));
}

void msg_puthdr(hdr) dns_hdr_t *hdr;
{
	msg_put16(hdr->id);
	msg_put16(hdr->flags);
	msg_put16(hdr->qdcount);
	msg_put16(hdr->ancount);
	msg_put16(hdr->nscount);
	msg_put16(hdr->arcount);
}

void put_name(name) char *name;
{
	char *s, *p;
	char buf[DNSNAMESIZE];

	u_strlcpy(buf, name, DNSNAMESIZE);
	p = buf;

	s = buf;
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
	uchar *p;
	int len;

	u_log(LG_FINE, "dns: sending message");

	addr.sin_family = AF_INET;
	addr.sin_port = htons(53);
	u_aton("8.8.8.8", &addr.sin_addr);

	p = msg + msghead;
	len = msgtail - msghead;
		
	u_log(LG_FINE, "sendto=%d",
	      sendto(dnsfd, p, len, 0, sa, sizeof(addr)));

	msg_dump();
} 

void send_req(req) dns_req_t *req;
{
	static int sent = 0;
	dns_hdr_t hdr;

	u_log(LG_DEBUG, "dns: sent total of %d requests", ++sent);

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

char *type_to_str(type)
{
	switch (type) {
	case DNS_TYPE_A:     return "    A";
	case DNS_TYPE_NS:    return "   NS";
	case DNS_TYPE_CNAME: return "CNAME";
	case DNS_TYPE_PTR:   return "  PTR";
	case DNS_TYPE_TXT:   return "  TXT";
	}
	return "other";
}

void dns_timeout(timer) u_io_timer *timer;
{
	dns_req_t *req = timer->priv;

	u_log(LG_DEBUG, "dns: request timed out");
	cache_add(req->name, DNS_TIMEOUT, "", NOW.tv_sec + 60);
	req_cb_call(req, DNS_TIMEOUT, NULL);
	req_del(req);
}

void dns_recv(iofd) u_io_fd *iofd;
{
	struct sockaddr addr;
	uint addrlen;
	int err;
	dns_hdr_t hdr;
	dns_req_t *req;
	dns_rr_t rr;
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
	req = req_find(hdr.id);
	if (req == NULL) {
		u_log(LG_ERROR, "dns_recv: no request with id %d!", (int)hdr.id);
		return;
	}

	if ((hdr.flags & DNS_MASK_RCODE) != DNS_RCODE_OK) {
		u_log(LG_ERROR, "dns_recv: response has RCODE 0x%x",
		      hdr.flags & DNS_MASK_RCODE);
		err = DNS_OTHER;
		if ((hdr.flags & DNS_MASK_RCODE) == DNS_RCODE_NAMEERR)
			err = DNS_NXDOMAIN;
		cache_add(req->name, err, "", NOW.tv_sec + 60);
		req_cb_call(req, err, NULL);
		req_del(req);
		return;
	}

	for (; hdr.qdcount>0; hdr.qdcount--)
		skip_question();

	err = -1;
	for (; hdr.ancount>0; hdr.ancount--) {
		msg_getrr(&rr);

		switch (rr.type) {
		case DNS_TYPE_A:
			in.s_addr = *((uint*)rr.rdata);
			strcpy(rdata, (char*)inet_ntoa(in));
			break;
		case DNS_TYPE_CNAME:
		case DNS_TYPE_PTR:
			p = rr.rdata;
			get_name_real(rdata, &p, 1);
			break;
		default:
			strcpy(rdata, "?");
			break;
		}

		u_log(LG_DEBUG, "%40s %s %8d %s",
		      rr.name, type_to_str(rr.type), rr.ttl, rdata);

		if (rr.type == req->type && err == -1) {
			err = DNS_OKAY;
			cache_add(req->name, err, rdata, NOW.tv_sec + rr.ttl);
			req_cb_call(req, err, rdata);
		}
	}

	if (err == -1) {
		u_log(LG_WARN, "dns_recv: didn't get record we wanted :(");
		cache_add(req->name, DNS_NXDOMAIN, "", NOW.tv_sec + 60);
		req_cb_call(req, DNS_NXDOMAIN, NULL);
	}

	req_del(req);
}

void u_dns_use_io(io) u_io *io;
{
	dnsio = io;

	if (dnssock != NULL) {
		u_log(LG_WARN, "u_dns_use_io called more than once!");
		dnssock->recv = NULL;
		dnssock->send = NULL;
		dnssock = NULL;
		/* io will delete it for us */
	}

	dnssock = u_io_add_fd(io, dnsfd);
	dnssock->recv = dns_recv;
	dnssock->send = NULL;
}

ushort request(type, name, cb, priv) char *name; void (*cb)(); void *priv;
{
	char res[DNSNAMESIZE];
	int status;
	dns_req_t *req;

	if ((status = cache_find(name, res)) >= 0) {
		u_log(LG_DEBUG, "dns: using cached result for %s", name);
		/* XXX: don't do callback right here */
		cb(status, res, priv);
		return 0;
	}

	req = req_find_by_name(name);

	if (req == NULL) {
		req = req_make(type, dns_timeout);
		u_strlcpy(req->name, name, DNSNAMESIZE);
		req_cb_add(req, cb, priv);
		send_req(req);
	} else {
		u_log(LG_DEBUG, "dns: chaining request for %s", name);
		req_cb_add(req, cb, priv);
	}

	return req->id;
}

ushort u_dns(name, cb, priv) char *name; void (*cb)(); void *priv;
{
	char buf[DNSNAMESIZE];

	if (!cb) {
		u_log(LG_WARN, "u_dns: ignoring DNS request with null callback");
		return 0;
	}

	u_log(LG_DEBUG, "dns: forward dns for %s", name);
	sprintf(buf, "%s.", name);

	return request(DNS_TYPE_A, buf, cb, priv);
}

ushort u_rdns(name, cb, priv) char *name; void (*cb)(); void *priv;
{
	char buf[DNSNAMESIZE];
	struct in_addr in;
	uchar *p;

	if (!cb) {
		u_log(LG_WARN, "u_rdns: ignoring DNS request with null callback");
		return 0;
	}

	u_log(LG_DEBUG, "dns: reverse dns for %s", name);
	u_aton(name, &in);
	p = (uchar*)&(in.s_addr);
	sprintf(buf, "%u.%u.%u.%u.in-addr.arpa.",
	        p[3], p[2], p[1], p[0]);

	return request(DNS_TYPE_PTR, buf, cb, priv);
}

void u_dns_cancel(id, cb, priv) ushort id; void (*cb)(); void *priv;
{
	dns_req_t *req = req_find(id);
	req_cb_del(req, cb, priv);
	if (req->cblist == NULL)
		req_del(req);
}

int init_dns()
{
	int i;

	for (i=0; i<512; i++)
		id_nfree[i] = 128;
	for (i=0; i<2048; i++)
		id_bitvec[i] = 0;

	u_list_init(&cache_by_recent);
	cache_by_name = u_map_new(1);

	dnsfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (dnsfd < 0)
		return -1;

	/* TODO: bind? */

	u_list_init(&reqs);
	return 0;
}

#include "ircd.h"

void u_linebuf_init(lb)
struct u_linebuf *lb;
{
	lb->pos = 0;
}

int u_linebuf_data(lb, data, size)
struct u_linebuf *lb;
char *data;
int size;
{
	if (lb->pos + size > U_LINEBUF_SIZE)
		return -1; /* linebuf full! */
	memcpy(lb->buf + lb->pos, data, size);
	lb->pos += size;
	return 0;
}

int u_linebuf_line(lb, dest, size)
struct u_linebuf *lb;
char *dest;
int size;
{
	char *s;
	int sz;
	s = (char*)memchr(lb->buf, '\n', lb->pos);
	if (s == NULL) return 0;
	if (s - lb->buf > size) return -1;
	sz = s - lb->buf;
	memcpy(dest, lb->buf, sz);
	u_memmove(lb->buf, s + 1, U_LINEBUF_SIZE - sz - 1);
	lb->pos -= sz + 1;
	return sz;
}

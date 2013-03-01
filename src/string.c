void u_memmove_lower(dest, src, n)
char *dest, *src;
int n;
{
	while (n --> 0) {
		*dest++ = *src++;
	}
}

void u_memmove_upper(dest, src, n)
char *dest, *src;
int n;
{
	src += n;
	dest += n;
	while (n --> 0) {
		*--dest = *--src;
	}
}

void u_memmove(dest, src, n)
char *dest, *src;
int n;
{
	if (dest < src)
		u_memmove_lower(dest, src, n);
	if (dest > src)
		u_memmove_upper(dest, src, n);
}

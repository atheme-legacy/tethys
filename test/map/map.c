#include "ircd.h"

#define LINESIZE 4096

u_map *map;

static void do_dump(u_map *map, void *k, void *v, void *priv)
{
	printf("%s=%s\n", k, v);
}

int main(int argc, char *argv[])
{
	char line[LINESIZE], *s, *p;
	int running = 1;
	size_t sz;

	map = u_map_new(1);

	while (running && !feof(stdin)) {
		fgets(line, LINESIZE, stdin);

		s = line;
		while (*s && isspace(*s)) /* move s to first non-space */
			s++;
		if ((p = strchr(s, '\n')) != NULL) /* cut off EOL */
			*p = '\0';

		fprintf(stderr, ">>> %s\n", s);

		switch (s[0]) {
		case 'q': /* quit */
			puts("bye");
			running = 0;
			break;

		case 'd': /* dump */
			u_map_each(map, do_dump, NULL);
			break;

		case 'D': { /* dump 2 */
			u_map_each_state state;
			char *k;
			void *v;

			U_MAP_EACH(&state, map, k, v)
				printf("%s=%s\n", k, v);
			break;
		}

		case '+': /* insert */
			p = strchr(s, '=');
			if (p == NULL) {
				puts("syntax error");
				break;
			}
			*p++ = '\0';
			u_map_set(map, s+1, strdup(p));
			break;

		case '-': /* delete */
			p = u_map_del(map, s+1);
			puts(p);
			free(p);
			break;

		case '?': /* test */
			puts(u_map_get(map, s+1) == NULL ? "no" : "yes");
			break;

		case '*': /* debug */
			u_map_dump(map);
			break;

		case '=': /* get */
			p = u_map_get(map, s+1);
			puts(p ? p : "");
			break;

		default:
			puts("?");
		}
	}
}

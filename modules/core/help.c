/* Tethys, core/help.c -- HELP command
   Copyright (C) 2014 Elizabeth Myers

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* Change this for longer/shorter help lines 
 * Anything in the help files longer than this will be truncated
 */
#define HELPLINESIZE 81


/* Storage place for all the help info */
typedef struct {
	time_t mtime; /* Last modified time of the help entry from stat() */
	mowgli_list_t lines; /* Lines for help */
} help_t;

/* The basic design is to store commands in these dicts. They point to
 * help_t's */
static mowgli_patricia_t *oper_help; /* opers commands */
static mowgli_patricia_t *user_help; /* peon commands */

/* Prototypes */
static void help_destroy(const char *, void *, void *);
static inline void read_lines(FILE *, mowgli_list_t *);
static inline void delete_lines(mowgli_list_t *);
static mowgli_list_t * find_help(const char *, bool);

static int module_init(u_module *m)
{
	oper_help = mowgli_patricia_create(ascii_canonize);
	user_help = mowgli_patricia_create(ascii_canonize);

	return 0;
}

static void module_deinit(u_module *m)
{
	mowgli_patricia_destroy(oper_help, &help_destroy, NULL);
	mowgli_patricia_destroy(user_help, &help_destroy, NULL);
}

static inline void read_lines(FILE *f, mowgli_list_t *lines)
{
	/* From a given file opened at f, read in all the help into list lines */

	int n;
	char line[HELPLINESIZE];
	line[HELPLINESIZE - 1] = '\0';

	while (fgets(line, sizeof(line) - 1, f) != NULL) {
		n = strlen(line);

		if (line[n-2] == '\r') /* Windows */
			line[n-2] = '\0';
		else if (line[n-1] == '\n' || line[n-1] == '\r') /* Linux / OS X */
			line[n-1] = '\0';

		mowgli_list_add(lines, strdup(line));
	}
}

static inline void delete_lines(mowgli_list_t *lines)
{
	/* Free all the lines contained in list lines */

	mowgli_node_t *n, *tn;

	/* Take the list AND THROW IT ON THE GROUND
	 * (IT'S NO LONGER PART OF OUR SYSTEM)
	 */
	MOWGLI_LIST_FOREACH_SAFE(n, tn, lines->head) {
		free(n->data);
		mowgli_list_delete(n, lines);
	}
}

static mowgli_list_t *find_help(const char *cmd, bool is_oper)
{
	/* Find a given help list. Returns NULL if there is none, else a
	 * pointer to the correct help */

	char path[PATH_MAX];
	bool exists;
	struct stat s;
	FILE *f;
	int err = 0;
	mowgli_patricia_t *tree = (is_oper ? oper_help : user_help);
	help_t *help = NULL;

	snprintf(path, sizeof(path), "help/%s/%s",
			(is_oper ? "opers" : "users"), cmd);

	exists = (stat(path, &s) == 0);
	if (!exists)
		err = errno;

	help = mowgli_patricia_retrieve(tree, cmd);

	if (!help && !exists)
		return NULL;

	if (!help) {
		if (!(f = fopen(path, "r")))
			return NULL;
		help = malloc(sizeof(*help));
		mowgli_list_init(&help->lines);
		mowgli_patricia_add(tree, cmd, help);
		goto read_help;
	}

	if (!exists) {
		if (err == ENOENT || err == ENOTDIR) {
			/* Delete the help if the file's gone */
			u_log(LG_DEBUG, "Deleting help item %s", cmd);
			delete_lines(&help->lines);
			mowgli_patricia_delete(tree, cmd);
			free(help);
			return NULL;
		}

		/* non-fatal and possibly transient error */
		return &help->lines;
	}

	/* cached fresh or can't fopen somehow (weird race conditions) */
	if (help->mtime == s.st_mtime || !(f = fopen(path, "r")))
		return &help->lines;

	delete_lines(&help->lines);

read_help:
	u_log(LG_DEBUG, "Loading %s help from %s", cmd, path);

	read_lines(f, &help->lines);
	fclose(f);

	help->mtime = s.st_mtime;

	return &help->lines;
}

static void help_destroy(const char *key, void *data, void *unused)
{
	help_t *help = (help_t *)data;

	delete_lines(&help->lines);
	free(help);
}

static inline int filter_cmd(int ch)
{
	/* Replace colons with dashes so IRC parsers don't choke */ 
	return (isalnum(ch) ? tolower(ch) : (ch == ':' ? '-' : '\0'));
}

static int c_lu_help(u_sourceinfo *si, u_msg *msg)
{
	char *cmd = msg->argv[0];
	mowgli_list_t *lines = NULL;
	mowgli_node_t *n;

	if (!str_transform(cmd, &filter_cmd))
		return u_src_num(si, ERR_HELPNOTFOUND, cmd);

	if (SRC_IS_OPER(si))
		lines = find_help(cmd, true);

	if ((!lines && !(lines = find_help(cmd, false))) ||
	   (mowgli_list_size(lines) == 0))
		return u_src_num(si, ERR_HELPNOTFOUND, cmd);

	/* Spew the lines */ 
	u_src_num(si, RPL_HELPSTART, cmd, lines->head->data);
	MOWGLI_LIST_FOREACH(n, lines->head->next)
		u_src_num(si, RPL_HELPTXT, cmd, n->data);

	u_src_num(si, RPL_ENDOFHELP, cmd);

	return 0;
}

static u_cmd help_cmdtab[] = {
	{ "HELP", SRC_LOCAL_USER,         c_lu_help, 1, U_RATELIMIT_MID },
	{ }
};

TETHYS_MODULE_V1(
	"core/help", "Elizabeth Myers", "HELP command",
	module_init, module_deinit, help_cmdtab);

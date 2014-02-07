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
typedef struct
{
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

	char line[HELPLINESIZE];
	line[HELPLINESIZE - 1] = '\0';

	/* Read lines from a given file into a list */
	while (fgets(line, sizeof(line) - 1, f) != NULL)
	{
		/* Chop off line ending */
		if (line[strlen(line) - 2] == '\r')
			/* Windows */
			line[strlen(line) - 2] = '\0';
		else if (line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == '\r')
			/* Linux/Mac OS X */
			line[strlen(line) - 1] = '\0';

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
	MOWGLI_LIST_FOREACH_SAFE(n, tn, lines->head)
	{
		free(n->data);
		mowgli_list_delete(n, lines);
	}
}

static mowgli_list_t * find_help(const char *cmd, bool is_oper)
{
	/* Find a given help list. Returns NULL if there is none, else a
	 * pointer to the correct help */

	char path[PATH_MAX];
	bool exists;
	struct stat s;
	FILE *f;
	int err = 0;
	mowgli_patricia_t *tree = is_oper ? oper_help : user_help;
	help_t *help = NULL;

	/* Build the path string */
	snprintf(path, sizeof(path), "help/%s/%s",
			(is_oper ? "opers" : "users"), cmd);

	exists = (stat(path, &s) == 0);
	if (!exists)
		/* Make a copy */
		err = errno;

	if (!(help = mowgli_patricia_retrieve(tree, cmd)))
	{
		if (!exists || !(f = fopen(path, "r")))
			return NULL;

		help = malloc(sizeof(help_t));
		mowgli_list_init(&help->lines);
		read_lines(f, &help->lines);

		fclose(f);

		help->mtime = s.st_mtime;

		mowgli_patricia_add(tree, cmd, help);
	}
	else
	{
		if (!exists)
		{
			if (err == ENOENT || err == ENOTDIR)
			{
				/* Delete the help if the file's gone */
				delete_lines(&help->lines);
				mowgli_patricia_delete(tree, cmd);
				free(help);

				return NULL;
			}
			else
				/* A non-fatal and possibly transient error */
				return &help->lines;
		}

		if(help->mtime == s.st_mtime)
			/* File is unmodified 
			 * XXX doesn't check for usec/nsec level precision. I 
			 * don't consider this a major problem though. */
			return &help->lines;

		if (!(f = fopen(path, "r")))
			/* Return old copy */
			return &help->lines;

		/* Out with the old, in with the new */
		delete_lines(&help->lines);
		read_lines(f, &help->lines);

		fclose(f);

		help->mtime = s.st_mtime;
	}

	return &help->lines;
}

static void help_destroy(const char *key, void *data, void *unused)
{
	help_t *help = (help_t *)data;

	delete_lines(&help->lines);
	free(help);
}

static int c_lu_help(u_sourceinfo *si, u_msg *msg)
{
	const char *cmd = msg->argv[0], *c = cmd;
	char cmd_lower[strlen(cmd) + 1], *d = cmd_lower;
	mowgli_list_t *lines = NULL;
	mowgli_node_t *n;

	for ( ; *c; ++c, ++d)
	{
		/* Downcase (case-sensitive FS etc.) and strip out non-alnum */
		if (!isalnum(*c))
			/* I think it's better to return an error */
			return u_src_num(si, ERR_HELPNOTFOUND, cmd);

		*d = tolower(*c);
	}
	*d = '\0';

	/* Repoint */
	cmd = cmd_lower;

	/* Check for oper status */
	if (si->u->mode & UMODE_OPER)
		lines = find_help(cmd, true);

	if ((!lines && !(lines = find_help(cmd, false))) ||
	   (mowgli_list_size(lines) == 0))
		return u_src_num(si, ERR_HELPNOTFOUND, cmd);

	/* Spew the lines */ 
	u_src_num(si, RPL_HELPSTART, cmd, lines->head->data);
	MOWGLI_LIST_FOREACH(n, lines->head->next)
	{
		u_src_num(si, RPL_HELPTXT, cmd, n->data);
	}

	u_src_num(si, RPL_ENDOFHELP, cmd);

	return 0;
}

static u_cmd help_cmdtab[] = {
	{ "HELP", SRC_LOCAL_USER,         c_lu_help, 1 },
	{ }
};

TETHYS_MODULE_V1(
	"core/help", "Elizabeth Myers", "HELP command",
	module_init, module_deinit, help_cmdtab);

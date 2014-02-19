/* Tethys, upgrade.c -- self-exec
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

const char *opt_upgrade;
mowgli_json_t *upgrade_json;

static int
_form_phoenix_args(const char *upgrade_fn, const char ***argv)
{
	static const char *_args[8];
	static char _verbosity[16] = "-";
	static char _port[6];

	int i = 0;
	int level = LG_INFO;
	char *vflag = _verbosity + 1;

	_args[i++] = main_argv0;

	if (level < u_log_level) {
		while (level++ < u_log_level)
			*vflag++ = 'v';
		*vflag++ = 0;
		_args[i++] = _verbosity;
	}

	if (opt_port >= 0) {
		sprintf(_port, "%d", opt_port);
		_args[i++] = "-p";
		_args[i++] = _port;
	}

	if (upgrade_fn) {
		_args[i++] = "-U";
		_args[i++] = upgrade_fn;
	}

	_args[i++] = NULL;

	*argv = _args;
	return 0;
}

/* begin_upgrade
 * -------------
 * Called to begin an upgrade. Re-execs. Does not return on success.
 */

static void
_json_append(mowgli_json_output_t *out, const char *str, size_t len)
{
	FILE *f = out->priv;
	if (fwrite(str, len, 1, f) != 1) {
		/* XXX */
		abort();
	}
}

static void
_json_append_char(mowgli_json_output_t *out, const char c)
{
	_json_append(out, &c, sizeof(c));
}

int
begin_upgrade(void)
{
	int err;
	const char **argv;
	FILE *f;
	mowgli_json_output_t json_out = {};
	u_hook *h_dump;

	/* Make sure the file doesn't currently exist. */
	if (unlink(UPGRADE_FILENAME) < 0 && errno != ENOENT)
		return -1;

	f = fopen(UPGRADE_FILENAME, "wb");
	if (!f)
		return -1;

	/* Open database */
	upgrade_json = mowgli_json_create_object();

	/* Call each unit and give it an opportunity to dump information */
#define DUMP(fn) if ((err = (fn)()) < 0) goto error
	DUMP(dump_user);
	DUMP(dump_server);
	DUMP(dump_chan);

	h_dump = u_hook_get(HOOK_UPGRADE_DUMP);
	if (u_hook_first(h_dump, NULL)) {
		/* return non-NULL to abort */
		return -1;
	}

	json_out.priv        = f;
	json_out.append      = _json_append;
	json_out.append_char = _json_append_char;

	mowgli_json_serialize(upgrade_json, &json_out, 1 /* DEBUG PRETTY */);

	mowgli_json_decref(upgrade_json);
	upgrade_json = NULL;

	fclose(f);

	/* Launch successor */
	if ((err = _form_phoenix_args(UPGRADE_FILENAME, &argv)) < 0)
		goto error;

	return execvp(argv[0], (char**)argv);

error:
	if (upgrade_json) {
		mowgli_json_decref(upgrade_json);
		upgrade_json = NULL;
	}
	return err;
}

/* finish_upgrade
 * --------------
 * Called when all modules are initialized and have had a chance to make use of
 * upgrade data.
 */
int
finish_upgrade(void)
{
	if (upgrade_json) {
		u_hook *h_restore = u_hook_get(HOOK_UPGRADE_RESTORE);
		if (u_hook_first(h_restore, NULL)) {
			/* return non-NULL to abort */
			return -1;
		}

		mowgli_json_decref(upgrade_json);
		upgrade_json = NULL;
		//unlink(opt_upgrade);
	}
	opt_upgrade = NULL;
	return 0;
}

/* abort_upgrade
 * -------------
 * In the event that restore fails, we re-execute ourself in non-upgrade mode.
 * One could design the restore system to be transactional, but that creates a
 * much greater degree of complexity. The objective of this system is to be
 * simple and maintainable. Strange though double-exec may be, it's a simple
 * and effective failsafe. Since the upgrade dump is written and read by (more
 * or less) the same code, failures really shouldn't happen. But in the
 * unlikely event that that does happen, we can at least continue operation
 * and accept new connections rather than simply crash.
 *
 * This means that the restore_ functions leak memory and result in undefined
 * state on failure. The program must exit when this occurs. This is comparable
 * to the init_ functions, so it matches the style of Tethys.
 *
 * Proper error handling is retained for the '_from_json' functions at this
 * time, since there is no clear motive to remove it.
 */
static void
_closefrom(int fd)
{
	int maxfd;

	/* This is POSIX and should be portable. */
	maxfd = (int)sysconf(_SC_OPEN_MAX);
	if (maxfd < 0)
		maxfd = 256; /* arbitrary */

	for (; fd<maxfd; ++fd)
		close(fd);
}

void
abort_upgrade(void)
{
	int err;
	const char **argv;

	if (!opt_upgrade)
		return;

	/* We have to close all fds before we exec. BSD has closefrom(3) which
	 * suffices, but Linux doesn't.
	 */
	_closefrom(3);

	if ((err = _form_phoenix_args(NULL, &argv)) < 0)
		abort();

	execvp(argv[0], (char**)argv);
	abort();
}

/* init_upgrade
 * ------------
 * Called on executable image bootup.
 */
int
init_upgrade(void)
{
	/* We have been executed to resume an upgrade (-U upgrade.db) */
	if (opt_upgrade) {
		/* The different units will load in their init functions when
		 * opt_upgrade != NULL.
		 */
		upgrade_json = mowgli_json_parse_file(opt_upgrade);
		if (!upgrade_json)
			return -1;
	}

	/* Not upgrading. */
	return 0;
}

/* vim: set noet: */

/* ircd-micro, crypto.c -- password hashing
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static char *crypt_alpha =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
static size_t crypt_alpha_len = 64;

static int gen_salt_3des(buf) char *buf;
{
	return -1;
}

static int hash_3des(buf, key, salt) char *buf, *key, *salt;
{
	return -1;
}

static int gen_salt_posix(buf) char *buf;
{
	strcpy(buf, "$p$");
	buf[3] = crypt_alpha[rand() % crypt_alpha_len];
	buf[4] = crypt_alpha[rand() % crypt_alpha_len];
	buf[5] = 0;
	return 0;
}

static int hash_posix(buf, key, salt) char *buf, *key, *salt;
{
	char *h;

	u_strlcpy(buf, salt, 4);
	if (strlen(salt) < 5 || !streq(buf, "$p$"))
		return -1;

	h = crypt(key, salt + 3);
	strcpy(buf, "$p$");
	u_strlcpy(buf + 3, h, CRYPTLEN - 3);
	return 0;
}

static int gen_salt_plain(buf) char *buf;
{
	buf[0] = 0;
	return 0;
}

static int hash_plain(buf, key, salt) char *buf, *key, *salt;
{
	u_strlcpy(buf, key, CRYPTLEN);
	return 0;
}

/* better hashes come first */
static u_crypto hashes[] = {
	{ "3des",    gen_salt_3des,    hash_3des    },
	{ "posix",   gen_salt_posix,   hash_posix   },
	{ "plain",   gen_salt_plain,   hash_plain   },
	{ "", NULL, NULL }
};

void u_crypto_gen_salt(buf) char *buf;
{
	u_crypto *cur;

	for (cur=hashes; cur->name[0]; cur++) {
		if (!cur->gen_salt)
			continue;
		if (cur->gen_salt(buf) < 0)
			continue;
		u_log(LG_DEBUG, "%s gen_salt %s", cur->name, buf);
		return;
	}

	u_log(LG_WARN, "Could not generate salt!");
	buf[0] = 0;
	return;
}

void u_crypto_hash(buf, key, salt) char *buf, *key, *salt;
{
	u_crypto *cur;

	for (cur=hashes; cur->name[0]; cur++) {
		if (!cur->hash)
			continue;
		if (cur->hash(buf, key, salt) < 0)
			continue;
		u_log(LG_DEBUG, "%s hash (salt %s) %s", cur->name, salt, buf);
		return;
	}

	u_log(LG_WARN, "Could not hash! (salt %s)", salt);
	buf[0] = 0;

	return;
}

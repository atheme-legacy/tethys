/* ircd-micro, crypto.h -- password hashing
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_CRYPTO_H__
#define __INC_CRYPTO_H__

/* generic buffer size */
#define CRYPTLEN 512

typedef struct u_crypto u_crypto;

struct u_crypto {
	char name[16];
	int (*gen_salt)(char *buf);
	int (*hash)(char *buf, char *key, char *salt);
};

extern void u_crypto_gen_salt(char *buf);
extern void u_crypto_hash(char *buf, char *key, char *salt);

#endif

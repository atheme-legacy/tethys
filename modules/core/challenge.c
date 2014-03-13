/* Tethys, core/challenge -- CHALLENGE command
   Copyright (C) 2014 Aaron Jones <aaronmdjones@gmail.com>

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

#ifdef HAVE_LIBCRYPTO

#include <openssl/evp.h>
#include <openssl/pem.h>

#define CHALLENGE_SIGNATURE_DIGEST_ALGORITHM	EVP_sha256()
#define CHALLENGE_EXPIRE_TIME			180
#define CHALLENGE_RANDOM_FILE			"/dev/urandom"

static int verify_signature(const unsigned char *data, unsigned int data_len, const unsigned char *sig, unsigned int sig_len, EVP_PKEY *pubkey)
{
	EVP_MD_CTX *mdctx = NULL;
	int ret = 0;

	if (data == NULL || sig == NULL) goto err;
	if (data_len < 1 || sig_len < 1) goto err;

	if (! (mdctx = malloc(sizeof(*mdctx)))) goto err;
	EVP_MD_CTX_init(mdctx);

	if (EVP_VerifyInit(mdctx, CHALLENGE_SIGNATURE_DIGEST_ALGORITHM) < 1) goto err;
	if (EVP_VerifyUpdate(mdctx, data, data_len) < 1) goto err;
	if (EVP_VerifyFinal(mdctx, sig, sig_len, pubkey) < 1) goto err;
	ret = 1;

err:
	if (mdctx) free(mdctx);
	return ret;
}

static void cleanup_challenge(u_oper_block *oper)
{
	oper->challenge_time = (time_t) 0;
	memset(oper->challenge, 0, sizeof(oper->challenge));
}

static int generate_challenge(unsigned char *challenge, int challenge_len)
{
	int rfd, ret;

	if ((rfd = open(CHALLENGE_RANDOM_FILE, O_RDONLY)) < 0)
		return 0;

	ret = read(rfd, challenge, challenge_len);
	close(rfd);

	return (ret == challenge_len) ? 1 : 0;
}

static int send_challenge(u_sourceinfo *si, u_oper_block *oper)
{
	char *challenge_b64 = calloc(1, base64_inflate_size(sizeof(oper->challenge)));
	if (challenge_b64 == NULL) return u_user_num(si->u, ERR_CHALLENGE_GENERATE);
	base64_encode(oper->challenge, sizeof(oper->challenge), challenge_b64, challenge_b64);
	u_user_num(si->u, RPL_CHALLENGE_STRING, challenge_b64);
	memset(challenge_b64, 0, strlen(challenge_b64));
	free(challenge_b64);
	return u_user_num(si->u, RPL_CHALLENGE_FINISH);
}

static int c_lu_challenge(u_sourceinfo *si, u_msg *msg)
{
	FILE *pubkey_file = NULL;
	EVP_PKEY *pubkey = NULL;
	unsigned int raw_sig_len = 0;
	unsigned char *raw_sig = NULL;

	u_oper_block *oper = u_get_oper_by_name(msg->argv[0]);
	if (oper == NULL)
		return u_user_num(si->u, ERR_NOOPERHOST);

	if (strlen(oper->pubkey) < 1)
		return u_user_num(si->u, ERR_CHALLENGE_NOPUBKEY);

	time_t cur_time = time(NULL);

	if (strcmp(msg->argv[1], "REQUEST") == 0) {
		if (oper->challenge_time > 0) {
			if ((cur_time - oper->challenge_time) < CHALLENGE_EXPIRE_TIME)
				return u_user_num(si->u, ERR_CHALLENGE_INPROG);

			cleanup_challenge(oper);
		}

		if (! generate_challenge(oper->challenge, sizeof(oper->challenge)))
			return u_user_num(si->u, ERR_CHALLENGE_GENERATE);

		oper->challenge_time = cur_time;
		return send_challenge(si, oper);
	}

	if (oper->challenge_time == 0) {
		u_user_num(si->u, ERR_CHALLENGE_NINPROG);
		goto cleanup;
	}
	if ((cur_time - oper->challenge_time) >= CHALLENGE_EXPIRE_TIME) {
		u_user_num(si->u, ERR_CHALLENGE_EXPIRED);
		goto cleanup;
	}

	if (! (raw_sig = calloc(1, base64_deflate_size(strlen(msg->argv[1]))))) {
		u_user_num(si->u, ERR_CHALLENGE_FAILURE);
		goto cleanup;
	}
	raw_sig_len = base64_decode(msg->argv[1], strlen(msg->argv[1]), raw_sig);

	if (! (pubkey_file = fopen(oper->pubkey, "r"))) {
		u_user_num(si->u, ERR_CHALLENGE_NOPUBKEY);
		goto cleanup;
	}
	if (! PEM_read_PUBKEY(pubkey_file, &pubkey, NULL, NULL)) {
		u_user_num(si->u, ERR_CHALLENGE_NOPUBKEY);
		goto cleanup;
	}
	if (! verify_signature(oper->challenge, sizeof(oper->challenge), raw_sig, raw_sig_len, pubkey)) {
		u_user_num(si->u, ERR_CHALLENGE_FAILURE);
		goto cleanup;
	}

	u_user_num(si->u, RPL_CHALLENGE_SUCCESS);
	oper_up(si, oper);

cleanup:
	if (raw_sig) free(raw_sig);
	if (pubkey_file) fclose(pubkey_file);
	if (pubkey) EVP_PKEY_free(pubkey);
	cleanup_challenge(oper);
	return 0;
}

#else

static int c_lu_challenge(u_sourceinfo *si, u_msg *msg)
{
	u_oper_block *oper = u_get_oper_by_name(msg->argv[0]);
	if (oper == NULL)
		return u_user_num(si->u, ERR_NOOPERHOST);

	if (strlen(oper->pubkey) < 1)
		return u_user_num(si->u, ERR_CHALLENGE_NOPUBKEY);

	return u_user_num(si->u, ERR_CHALLENGE_NOSUPPORT);
}

#endif /* HAVE_LIBCRYPTO */

static u_cmd challenge_cmdtab[] = {
	{ "CHALLENGE", SRC_LOCAL_USER, c_lu_challenge, 2 },
	{ }
};

TETHYS_MODULE_V1(
	"core/challenge", "Aaron Jones <aaronmdjones@gmail.com>",
	"CHALLENGE command", NULL, NULL, challenge_cmdtab);


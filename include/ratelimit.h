/* Tethys, src/ratelimit.h - rate limiting stuff
 *
 * Copyright (C) 2014 Elizabeth Myers.
 * This file is protected under the terms contained in the COPYING file in the
 * project root
 */

#ifndef __INC_RATELIMIT_H__
#define __INC_RATELIMIT_H__

typedef struct u_user u_user;

/* Flood rate tokens */
#define FLOODTOKENS 30

/* Grace period length */
#define GRACEPERIOD 15

/* Refil after this time of no activity */
#define REFILLPERIOD 10 

typedef struct {
	/* Token count */
	unsigned int tokens;

	/* WHO tokens left 
	 *
	 * Joins give one token. Parts and WHO queries take one.
	 * When the tokens are gone, normal flood limiting applies.
	 */
	unsigned int whotokens;

	/* Time of last token deduction
	 *
	 * When REFILLPERIOD has elapsed, we refill a token
	 */
	time_t last;
} u_ratelimit_t;

typedef struct {
	/* Number of tokens deducted per use */
	unsigned int deduction;

	/* Send LOAD2HI upon fail 
	 * If set false, it will disconnect the user
	 */
	bool warn;
} u_ratelimit_cmd_t;

/* No ratelimit */
#define U_RATELIMIT_NONE { 0, false }

/* Standard rate limit (PRIVMSG etc) */
#define U_RATELIMIT_STD { 1, false }

/* Rate limit more sensitive things (NICK etc) */
#define U_RATELIMIT_MID { 2, false }

/* For things like WHOIS etc */
#define U_RATELIMIT_HI { 5, true }


/* Functions */
void u_ratelimit_init(u_user *user);
bool u_ratelimit_allow(u_user *user, u_ratelimit_cmd_t *deduct, const char *cmd);
void u_ratelimit_who_credit(u_user *user);
void u_ratelimit_who_deduct(u_user *user);

#endif

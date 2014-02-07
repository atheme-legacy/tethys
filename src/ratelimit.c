/* Tethys, src/ratelimit.c - rate limiting stuff
 *
 * Copyright (C) 2014 Elizabeth Myers.
 * This file is protected under the terms contained in the COPYING file in the
 * project root
 */

#include "ircd.h"

void u_ratelimit_init(u_user *user)
{
	user->limit.tokens = FLOODTOKENS;
	user->limit.last = NOW.tv_sec;
}

/* Should we allow a given command? */
bool u_ratelimit_allow(u_user *user, u_ratelimit_cmd_t *deduct, const char *cmd)
{
	time_t lastrate, numtokens;

	if ((strcasecmp(cmd, "WHO") == 0) && (user->limit.whotokens > 0))
	{
		/* Compensate for WHO */
		u_ratelimit_who_deduct(user);
		return true;
	}

	lastrate = NOW.tv_sec - user->limit.last;
	numtokens = lastrate / REFILLPERIOD;

	/* Update the last executed time */
	user->limit.last = NOW.tv_sec;

	/* Grant tokens if needed */
	user->limit.tokens += numtokens;
	if (user->limit.tokens > FLOODTOKENS)
		user->limit.tokens = FLOODTOKENS;

	/* Subtract tokens */
	if ((user->limit.tokens <= deduct->deduction) ||
	    (user->limit.tokens - deduct->deduction) == 0)
	{
		user->limit.tokens = 0;

		u_log(LG_WARN, "User %U flooding!", user);

		/* XXX do a kill */
		return false;
	}

	return true;
}

/* Credit a user for join */
void u_ratelimit_who_credit(u_user *user)
{
	user->limit.whotokens++;
}

/* Deduct a user for part */
void u_ratelimit_who_deduct(u_user *user)
{
	if (user->limit.whotokens > 0)
		user->limit.whotokens--;
}

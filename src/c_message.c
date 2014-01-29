/* Tethys, c_message.c -- message propagation
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

u_cmd c_message[] = {
	{ "PRIVMSG",   CTX_USER,     m_message, 2 },
	{ "NOTICE",    CTX_USER,     m_message, 2 },
	{ "PRIVMSG",   CTX_SERVER,   m_message, 2 },
	{ "NOTICE",    CTX_SERVER,   m_message, 2 },

	{ "" },
};

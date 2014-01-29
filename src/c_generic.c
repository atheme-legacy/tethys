/* Tethys, c_generic.c -- commands with hunted parameters
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

u_cmd c_generic[] = {
	{ "PING",        CTX_USER,    m_ping,        1 },
	{ "PONG",        CTX_USER,    m_ping,        0 },
	{ "PING",        CTX_SERVER,  m_ping,        1 },
	{ "PONG",        CTX_SERVER,  m_pong,        2 },

	{ "AWAY",        CTX_USER,    m_away,        0 },
	{ "AWAY",        CTX_SERVER,  m_away,        0 },

	{ "" }
};

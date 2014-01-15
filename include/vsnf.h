/* Tethys, vsnf.h -- string formatter
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_VSNF_H__
#define __INC_VSNF_H__

#define FMT_USER    1
#define FMT_SERVER  2
#define FMT_LOG     3
#define FMT_DEBUG   4  /* LOG + some extras */

extern int vsnf(int type, char *buf, uint size, const char *fmt, va_list va);
extern int snf(int, char*, uint, char*, ...);

/*
   It's like vsnprintf with IRC-suitable additions. This will a)
   guard against buffer overflow problems, since 4.3 BSD does not have
   vsnprintf, only vsprintf, and b) make format strings a lot simpler,
   e.g. no more ("%s!%s@%s", u->nick, u->ident, u->host), which becomes
   just ("%H", u) here.

   vsnf is not simply vsnprintf with extra IRC stuff. The vsnf format
   strings are much more limited than printf-family formats. However, the
   similarities are enough that people should be comfortable with these.

   Formats are as follows:

   %s %d %u %o %x %p %% %c
                   These behave exactly as their printf counterparts
                   for all format types.

   %T              This takes no argument and simply inserts a timestamp
                   into the output string. For USER and SERVER, this is
                   just NOW.tv_sec. For LOG, this is ctime(NOW.tv_sec).

   %U, u_user*     Prints a nickname for USER and LOG, and a UID for
                   SERVER.

   %H, u_user*     Prints a hostmask for USER and LOG, and a UID for
                   SERVER.

   %S, u_server*   Prints the server name for USER and LOG, and a SID
                   for SERVER.

   %C, u_char*     Prints the channel name in all cases.

   Only a limited subset of format specifier parameters (width, etc.) are
   supported. These go between % and the letter. I don't know how to
   classify these, so here's a table of what's implemented and what's not:

      IMPLEMENTED   FORGET ABOUT IT
      %15s          %*s
      %03d          %+3d
                    % 5d

 */

#endif

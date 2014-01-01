dnl ircd-micro, configure.c -- autoconf input
dnl Copyright (C) 2013 Alex Iadicicco
dnl
dnl This file is protected under the terms contained
dnl in the COPYING file in the project root

AC_INIT([ircd-micro], [0.1], [https://github.com/aji/ircd-micro])

AC_CONFIG_HEADER([include/autoconf.h])
AC_PREFIX_DEFAULT(~/ircd)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S

AC_CHECK_HEADER_STDBOOL

AC_SEARCH_LIBS(crypt, crypt, [AC_DEFINE([HAVE_CRYPT], [], [If crypt()])])

BUILDSYS_SHARED_LIB
BUILDSYS_PROG_IMPLIB

BUILDSYS_INIT
BUILDSYS_TOUCH_DEPS

AC_CONFIG_FILES([buildsys.mk extra.mk])

AC_OUTPUT

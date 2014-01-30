dnl Tethys, configure.c -- autoconf input
dnl Copyright (C) 2013 Alex Iadicicco
dnl
dnl This file is protected under the terms contained
dnl in the COPYING file in the project root

AC_INIT([Tethys], [0.1-alpha1], [https://github.com/atheme/tethys])

AC_CONFIG_HEADER([include/autoconf.h])
AC_PREFIX_DEFAULT(~/ircd)

AC_MSG_CHECKING([if you read the readme])
if ! test -f libmowgli-2/Makefile; then
	AC_MSG_RESULT(no)
	AC_ERROR([please read the readme before trying to build from git])
fi
AC_MSG_RESULT(yes)

MOWGLI="libmowgli-2/src/libmowgli"

MOWGLI_CFLAGS="-I../$MOWGLI -I../../$MOWGLI"
MOWGLI_LIBS="-L../$MOWGLI -L../../$MOWGLI -lmowgli-2"
LIBMOWGLI="libmowgli-2"
AC_CONFIG_SUBDIRS([libmowgli-2])

AC_SUBST(MOWGLI_CFLAGS)
AC_SUBST(MOWGLI_LIBS)
AC_SUBST(LIBMOWGLI)

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

echo "touching numeric.h"
touch src/numeric.h

AC_CONFIG_FILES([buildsys.mk extra.mk])

AC_OUTPUT

echo "
${PACKAGE_NAME} ${PACKAGE_VERSION} configuration
     Install prefix : ${prefix}
     CFLAGS         : ${CFLAGS}
     LIBS           : ${LIBS}

Type make to build, and make install to install.
"

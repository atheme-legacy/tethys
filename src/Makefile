.PHONY: version.c
PROG = tethys
SRCS = numeric.c \
	auth.c \
	c_generic.c \
	c_message.c \
	c_reg.c \
	c_server.c \
	c_user.c \
	chan.c \
	conf.c \
	conn.c \
	cookie.c \
	crypto.c \
	entity.c \
	hook.c \
	linebuf.c \
	log.c \
	map.c \
	mode.c \
	module.c \
	msg.c \
	sendto.c \
	server.c \
	upgrade.c \
	user.c \
	util.c \
	version.c \
	vsnf.c \
	main.c
DISTCLEAN = numeric.c numeric.h

build: all

include ../buildsys.mk
include ../extra.mk

CFLAGS += -Wall
CPPFLAGS += -I. -I../include
LDFLAGS += -rdynamic $(LDFLAGS_RPATH)

CFLAGS += $(MOWGLI_CFLAGS)
LIBS += $(MOWGLI_LIBS)

numeric.h numeric.c: numeric.tab
	@echo "Creating numeric.[ch]"
	awk -f numeric.awk < numeric.tab

version.c.dep:
	echo >version.c.dep

version.c:
	@echo "Creating version.c"
	sh version.sh

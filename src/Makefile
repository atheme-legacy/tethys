PROG = ircd-micro
EXT_DEPS = numeric.h
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

include ../extra.mk
include ../buildsys.mk

CFLAGS += -Wall
CPPFLAGS += -I. -I../include
LDFLAGS += -rdynamic $(LDFLAGS_RPATH)

CFLAGS += $(MOWGLI_CFLAGS)
LIBS += $(MOWGLI_LIBS)

build: all

numeric.h numeric.c: numeric.tab
	awk -f numeric.awk < numeric.tab
version.c: version.sh
	sh version.sh

MODULE = core

SRCS = \
	away.c \
	euid.c \
	hunted.c \
	invite.c \
	join.c \
	map.c \
	message.c \
	mode.c \
	module.c \
	nick.c \
	numeric.c \
	oper.c \
	part.c \
	pass.c \
	ping.c \
	quit.c \
	sid.c \
	ts6init.c \
	topic.c \
	user.c \
	who.c \
	whois.c

include ../../buildsys.mk
include ../../buildsys.module.mk
include ../../extra.mk

CFLAGS += -I../../include -I../../src

CFLAGS += $(MOWGLI_CFLAGS)
LIBS += $(LDFLAGS_RPATH)
LIBS += $(MOWGLI_LIBS)

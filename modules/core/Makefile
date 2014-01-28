MODULE = core

SRCS = \
	join.c \
	message.c \
	mode.c \
	nick.c \
	part.c \
	pass.c \
	quit.c \
	ts6init.c \
	user.c

include ../../buildsys.mk
include ../../buildsys.module.mk
include ../../extra.mk

CFLAGS += -I../../include -I../../src

CFLAGS += $(MOWGLI_CFLAGS)
LIBS += $(LDFLAGS_RPATH)
LIBS += $(MOWGLI_LIBS)

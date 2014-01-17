MODULE = extra

SRCS = logchan.c \
	pseudoclient.c \
	alias.c

include ../../buildsys.mk
include ../../buildsys.module.mk
include ../../extra.mk

CFLAGS += -I../../include -I../../src

CFLAGS += $(MOWGLI_CFLAGS)
LIBS += $(LDFLAGS_RPATH)
LIBS += $(MOWGLI_LIBS)

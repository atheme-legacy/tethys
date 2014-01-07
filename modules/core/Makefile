MODULE = core

SRCS = hello.c \
       c_42.c

include ../../extra.mk
include ../../buildsys.mk
include ../../buildsys.module.mk

CFLAGS += -I../../include -I../../src

CFLAGS += $(MOWGLI_CFLAGS)
LIBS += $(LDFLAGS_RPATH)
LIBS += $(MOWGLI_LIBS)

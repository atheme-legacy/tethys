MODULE = contrib

SRCS = hello.c \
       c_42.c

include ../../buildsys.mk
include ../../buildsys.module.mk
include ../../extra.mk

CFLAGS += -I../../include -I../../src

CFLAGS += $(MOWGLI_CFLAGS)
LIBS += $(LDFLAGS_RPATH)
LIBS += $(MOWGLI_LIBS)

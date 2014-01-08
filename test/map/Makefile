CFLAGS += -g -O0

CFLAGS += -I../../include -I../../src

MOWGLI = ../../libmowgli-2/src/libmowgli
CFLAGS += -I$(MOWGLI)
LDFLAGS += -L$(MOWGLI) -lmowgli-2

SRC = ../../src
LOG_STUBS = ../log_stubs.c

map: map.c $(LOG_STUBS) $(SRC)/map.c
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^

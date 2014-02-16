CFLAGS += -g -O0

CFLAGS += -I../../include -I../../src

MOWGLI = ../../libmowgli-2/src/libmowgli
CFLAGS += -I$(MOWGLI)
LDFLAGS += -L$(MOWGLI) -lmowgli-2

SRC = ../../src
LOG_STUBS = ../log_stubs.c

echo: echo.c $(LOG_STUBS) $(SRC)/conn.c
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^

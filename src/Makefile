# ircd-micro, Makefile
# Copyright (C) 2013 Alex Iadicicco
#
# This file is protected under the terms contained
# in the COPYING file in the project root

OUTPUT = ircd-micro
EXTRA = numeric.h \
	numeric.c \
	version.c
OBJS = numeric.o \
	auth.o \
	c_hunted.o \
	c_message.o \
	c_reg.o \
	c_server.o \
	c_user.o \
	chan.o \
	conf.o \
	conn.o \
	cookie.o \
	crypto.o \
	dns.o \
	io.o \
	linebuf.o \
	list.o \
	log.o \
	map.o \
	msg.o \
	sendto.o \
	server.o \
	trie.o \
	upgrade.o \
	user.o \
	util.o \
	version.o \
	vsnf.o \
	main.o

CFLAGS = -Wall -g -I. -I../include
LIBS = $(shell ld -lcrypt -o /dev/null 2>/dev/null && echo -lcrypt)
CC = cc
LD = cc

default: $(OUTPUT)
clean:
	rm -f $(EXTRA)
	rm -f $(OBJS)
	rm -f $(OUTPUT)

$(OUTPUT): $(EXTRA) $(OBJS)
	$(LD) -o $@ $(OBJS) $(LIBS)
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
numeric.h numeric.c: numeric.tab
	awk -f numeric.awk < numeric.tab
version.c: version.sh
	sh version.sh

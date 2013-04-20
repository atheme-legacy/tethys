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
	c_reg.o \
	c_user.o \
	chan.o \
	conf.o \
	conn.o \
	cookie.o \
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
	util.o \
	user.o \
	version.o \
	vsnf.o \
	main.o

CFLAGS = -Wall -g -I. -I../include
CC = cc
LD = cc

default: $(OUTPUT)
clean:
	rm -f $(EXTRA)
	rm -f $(OBJS)
	rm -f $(OUTPUT)

$(OUTPUT): $(EXTRA) $(OBJS)
	$(LD) -o $@ $(OBJS)
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
numeric.h numeric.c: numeric.tab
	awk -f numeric.awk < numeric.tab
version.c: version.sh
	sh version.sh

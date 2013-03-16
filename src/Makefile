OUTPUT = ircd-micro
OBJS = numeric.h \
	list.o \
	log.o \
	io.o \
	linebuf.o \
	msg.o \
	main.o \
	util.o \
	server.o \
	conf.o \
	user.o \
	conn.o \
	heap.o \
	trie.o \
	c_ureg.o

CFLAGS = -g -I. -I../include
CC = cc
LD = cc

default: $(OUTPUT)
clean:
	rm -f $(OBJS)
	rm -f $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(LD) -o $@ $(OBJS)
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
numeric.h: numeric.tab
	awk -f numeric.awk < $< > $@

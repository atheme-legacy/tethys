OUTPUT = ircd-micro
OBJS = hash.o list.o io.o linebuf.o msg.o main.o util.o \
	user.o toplev.o conn.o heap.o \
	c_nick.o c_user.o

INCLUDE = ../include
CFLAGS = -g -I$(INCLUDE) -DDEBUG
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

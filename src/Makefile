OUTPUT = ircd-micro
OBJS = hash.o list.o io.o linebuf.o msg.o main.o string.o \
	user.o toplev.o conn.o \
	c_nick.o c_user.o

INCLUDE = ../include
CFLAGS = -g -I$(INCLUDE)
CC = gcc
LD = gcc

default: $(OUTPUT)
clean:
	rm -f $(OBJS)
	rm -f $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(LD) -o $@ $(OBJS)
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

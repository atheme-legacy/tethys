OUTPUT = ircd-micro
OBJS = hash.o list.o io.o linebuf.o msg.o main.o string.o \
	user.o toplev.o connection.o \
	c_nick.o c_user.o

INCLUDE = ../include
CFLAGS = -g -I$(INCLUDE)
CC = cc
LD = cc

default: $(OUTPUT)
clean:
	rm -f $(OBJS)
	rm -f $(OUTPUT)

$(OUTPUT): $(OBJS)
	@echo '    LD  ' $@
	@$(LD) -o $@ $(OBJS)
.c.o:
	@echo '    CC  ' $@
	@$(CC) -c $(CFLAGS) -o $@ $<

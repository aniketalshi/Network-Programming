CC = gcc

LIBS = -lresolv -lnsl -lpthread\
    /home/stufs1/aalshi/cse533/stevens/unpv13e/libunp.a\

FLAGS = -g -O2
CFLAGS = ${FLAGS} -I /home/stufs1/aalshi/cse533/stevens/unpv13e/lib/

all: time_cli

time_cli: time_cli.o
	${CC} ${FLAGS} -o time_cli time_cli.o ${LIBS}

time_cli.o: time_cli.c
	${CC} ${CFLAGS} -c time_cli.c
	
.PHONY : clean
clean: 
	rm time_cli time_cli.o


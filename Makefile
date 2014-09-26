CC = gcc

LIBS = -lresolv -lnsl -lpthread\
    /home/stufs1/aalshi/cse533/stevens/unpv13e/libunp.a\

FLAGS = -g -O2
CFLAGS = ${FLAGS} -I /home/stufs1/aalshi/cse533/stevens/unpv13e/lib/

all: time_cli time_srv echo_srv echo_cli

time_srv: time_srv.o
	${CC} ${FLAGS} -o time_srv time_srv.o ${LIBS}
time_srv.o: time_srv.c
	${CC} ${CFLAGS} -c time_srv.c

time_cli: time_cli.o
	${CC} ${FLAGS} -o time_cli time_cli.o ${LIBS}
time_cli.o: time_cli.c
	${CC} ${CFLAGS} -c time_cli.c

echo_srv: echo_srv.o
	${CC} ${FLAGS} -o echo_srv echo_srv.o ${LIBS}
echo_srv.o: echo_srv.c
	${CC} ${CFLAGS} -c echo_srv.c

echo_cli: echo_cli.o
	${CC} ${FLAGS} -o echo_cli echo_cli.o ${LIBS}
echo_cli.o: echo_cli.c
	${CC} ${CFLAGS} -c echo_cli.c

.PHONY : clean
clean: 
	rm time_cli time_srv time_cli.o \
	time_srv.o echo_cli.o echo_srv.o


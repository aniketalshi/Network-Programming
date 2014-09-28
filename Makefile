CC = gcc

LIBS = -lresolv -lnsl -lpthread\
    /home/stufs1/aalshi/cse533/stevens/unpv13e/libunp.a\

FLAGS = -g -O2 -lm
CFLAGS = ${FLAGS} -I /home/stufs1/aalshi/cse533/stevens/unpv13e/lib/

all: client server time_cli time_srv echo_srv echo_cli

server: server.o
	${CC} ${FLAGS} -o server server.o ${LIBS}
server.o: server.c
	${CC} ${CFLAGS} -c server.c

client: client.o
	${CC} ${FLAGS} -o client client.o ${LIBS}
client.o: client.c
	${CC} ${CFLAGS} -c client.c

time_srv: time_srv.o
	${CC} ${FLAGS} -o time_srv time_srv.o ${LIBS}
time_srv.o: time_srv.c
	${CC} ${CFLAGS} -c time_srv.c

time_cli: time_cli.o
	${CC} ${FLAGS} -o time_cli time_cli.o ${LIBS}
time_cli.o: time_cli.c
	${CC} ${CFLAGS} -c time_cli.c

readline.o: /home/stufs1/aalshi/cse533/assign1/readline.c
	${CC} ${CFLAGS} -c /home/stufs1/aalshi/cse533/assign1/readline.c

echo_srv: echo_srv.o readline.o
	${CC} ${FLAGS} -o echo_srv echo_srv.o readline.o ${LIBS}
echo_srv.o: echo_srv.c
	${CC} ${CFLAGS} -c echo_srv.c

echo_cli: echo_cli.o readline.o
	${CC} ${FLAGS} -o echo_cli echo_cli.o readline.o ${LIBS}
echo_cli.o: echo_cli.c
	${CC} ${CFLAGS} -c echo_cli.c

.PHONY : clean
clean: 
	rm time_cli time_srv time_cli.o readline.o \
	time_srv.o echo_cli.o echo_srv.o client.o server.o


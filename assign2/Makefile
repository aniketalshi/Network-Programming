CC = gcc

LIBS = -lresolv -lsocket -lnsl -lpthread -lm\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\

FLAGS = -g -O2
CFLAGS = ${FLAGS} -I /home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

all: prifinfo_plus server client

structs.o: structs.c 
	${CC} ${CFLAGS} -c structs.c

prifinfo_plus: prifinfo_plus.o get_ifi_info_plus.o
	${CC} ${FLAGS} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${LIBS}
prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c

server: server.o structs.o get_ifi_info_plus.o controls.o
	${CC} ${FLAGS} -o server server.o structs.o controls.o get_ifi_info_plus.o ${LIBS}
server.o: server.c
	${CC} ${CFLAGS} -c server.c

client: client.o client.o get_ifi_info_plus.o controls.o
	${CC} ${FLAGS} -o client client.o structs.o controls.o get_ifi_info_plus.o ${LIBS}
client.o: client.c
	${CC} ${CFLAGS} -c client.c

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

rtt.o: rtt.c
	${CC} ${CFLAGS} -c rtt.c

controls.o: controls.c
	${CC} ${CFLAGS} -c controls.c

clean:
	rm client prifinfo_plus server prifinfo_plus.o get_ifi_info_\
	plus.o server.o structs.o client.o controls.o


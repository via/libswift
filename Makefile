CFLAGS= -O0 -g
CC=gcc

libswift.o: swift.c swift.h
	${CC} ${CFLAGS} -o libswift.o -c swift.c `pkg-config libcurl --cflags --libs`

test: test.c libswift.o
	${CC} ${CFLAGS} -o test test.c libswift.o  `pkg-config libcurl --cflags --libs`

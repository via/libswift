CFLAGS= -O0 -g  -Wall `pkg-config libcurl --cflags --libs` -I. -DUNITTEST
CC=gcc


libswift.o: swift.c swift.h swift_private.h
	${CC} ${CFLAGS} -o libswift.o -c swift.c 

tests/test_swift: tests/test_swift.c libswift.o
	${CC} ${CFLAGS} -o tests/test_swift tests/test_swift.c libswift.o `pkg-config check --cflags --libs` 

tests: tests/test_swift

client: client.c libswift.o
	${CC} ${CFLAGS} -o client client.c libswift.o 

clean:
	-rm *.o
	-rm client
	-rm tests/test_swift

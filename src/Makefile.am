lib_LTLIBRARIES = libswift.la
bin_PROGRAMS = swiftclient
include_HEADERS = swift.h

libswift_la_SOURCES = swift.h swift.c swift_private.h
libswift_la_LIBADD = $(CURL_LIBS)

swiftclient_SOURCES = client.c swift.h
swiftclient_LDADD = $(CURL_LIBS) libswift.la


AM_CFLAGS = $(CURL_CFLAGS)

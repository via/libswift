if UNITTEST
TESTS = check_swift
check_PROGRAMS = check_swift
endif
check_swift_SOURCES = test_swift.c $(top_builddir)/src/swift_private.h $(top_builddir)/src/swift.h $(top_builddir)/src/curl_mockups.h
AM_CFLAGS = @check_CFLAGS@
LDADD = @check_LIBS@ $(top_builddir)/src/libswift.la

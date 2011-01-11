if UNITTEST
TESTS = check_swift
check_PROGRAMS = check_swift
endif
if INTEGRATION
TESTS = check_integration
check_PROGRAMS = check_integration
endif
check_swift_SOURCES = test_swift.c $(top_builddir)/src/swift_private.h $(top_builddir)/src/swift.h $(top_builddir)/src/curl_mockups.h
check_integration_SOURCES = integration_test.c $(top_builddir)/src/swift_private.h $(top_builddir)/src/swift.h
AM_CFLAGS = @check_CFLAGS@
LDADD = @check_LIBS@ $(top_builddir)/src/libswift.la

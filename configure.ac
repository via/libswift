#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([libswift], [0.01], [matthew.via@mailtrust.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
LT_INIT
AC_CONFIG_SRCDIR([src/swift.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile swift.pc])
AC_CONFIG_MACRO_DIR([m4])

AC_ARG_ENABLE([unittest], AS_HELP_STRING([--enable-unittest],[enable exposed internal funcs for unit testing]))
AM_CONDITIONAL([UNITTEST], [test "x$enable_unittest" = "xyes"])
AS_IF([test "x$enable_unittest" = "xyes"], [
       AC_DEFINE([UNITTEST], [1], [Expose static functions for unit testing])
       ])

AC_ARG_ENABLE([integration],
            AS_HELP_STRING([--enable-integration=username,password,url],[enable
             integration testing with given server options]),
             [integration=$enableval],
             [integration=no])
AS_IF([test "x$integration" != "xno"], [
       AS_IF([test "x$enable_unittest" = "xyes"], [
              integration=no
              ])
       ])
AM_CONDITIONAL([INTEGRATION], [test "x$integration" != "xno"])
AS_IF([test "x$integration" != "xno"], [
       AC_DEFINE_UNQUOTED([INTEGRATION], "$integration", [Login information for
                  integration test server])
       ])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C89
AC_PROG_CXX
AC_PROG_LIBTOOL

# Checks for libraries.
PKG_CHECK_MODULES(CURL, libcurl)
AS_IF([test "x$enable_unittest" = "xyes" -o "x$integration" != "xno" ], [
  PKG_CHECK_MODULES([check], [check >= 0.9.4])
  ])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strchr strtol])

AC_OUTPUT

AC_PREREQ([2.10])
AC_INIT([ambientlight], [1.0.0], [matt@majenko.co.uk])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects no-dependencies])
AC_CONFIG_MACRO_DIR([m4])

AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC

AC_SEARCH_LIBS([XDefaultVisual], [X11], [], [
    AC_MSG_ERROR([libX11 is required])
])

AC_SEARCH_LIBS([XShmCreateImage], [Xext], [], [
    AC_MSG_ERROR([libXm is required])
], [])

AC_CHECK_HEADERS([errno.h])
AC_CHECK_HEADERS([sys/shm.h])
AC_CHECK_HEADERS([X11/Xlib.h])
AC_CHECK_HEADERS([X11/Xutil.h])
AC_CHECK_HEADERS([X11/extensions/XShm.h], [], [
    AC_MSG_ERROR([Cannot find XShm.h])
], [
#include <X11/Xlib.h>
])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([netdb.h])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT


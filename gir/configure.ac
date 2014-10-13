AC_PREREQ(2.65)

AC_INIT([ghinawa], [1.0.0], [o-takashi@sakamocchi.jp])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.13 foreign])
AM_SILENT_RULES([yes])

AC_PROG_CC
LT_INIT

AM_PATH_GLIB_2_0([2.32.4], [], [], [gobject])

GOBJECT_INTROSPECTION_REQUIRE([1.32.1])
GTK_DOC_CHECK([1.18-2])

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  doc/Makefile
  doc/reference/Makefile
  doc/reference/version.xml
])

AC_OUTPUT

# Audiconf 2.65 or later because Automake 1.13 requires.
AC_PREREQ(2.65)

# Use -release option for libtool
# See:
# http://www.gnu.org/software/libtool/manual/html_node/Release-numbers.html
m4_define([my_major], [0])
m4_define([my_minor], [3])
m4_define([my_micro], [0])
m4_define([my_version],	[my_major.my_minor.my_revision])

m4_define([my_current], [m4_eval(my_major * 100 + my_minor)])
m4_define([my_revision],[my_major])
m4_define([my_age],     [my_minor])
m4_define([my_iface],	[my_current:my_revision:my_age])

# Initialize Autoconf
AC_INIT([hinawa], [my_version], [o-takashi@sakamocchi.jp])
# The directory for helper scripts
AC_CONFIG_AUX_DIR([config])
# The directory for M4 macros
AC_CONFIG_MACRO_DIR([m4])

# The directory for sources
AC_CONFIG_SRCDIR([src])
# The header files for variables with AC_DEFINE
AC_CONFIG_HEADERS([config.h])

# Automake 1.13 or later
AM_INIT_AUTOMAKE([1.13 foreign])
# Don't output comand lines
AM_SILENT_RULES([yes])

# Detect C language compiler
AC_PROG_CC

# Libtools
AC_PROG_LIBTOOL

# Initialize Libtools
LT_INIT

# Define library release
LT_IFACE="my_iface"
AC_SUBST(LT_IFACE)

# Glib 2.32 or later
AM_PATH_GLIB_2_0([2.32.4], [], [], [gobject])

# GObject introspection 1.32.1 or later
GOBJECT_INTROSPECTION_REQUIRE([1.32.1])

# GTK-Doc 1.18-2 or later
GTK_DOC_CHECK([1.18-2])

# The files generated from *.in
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  doc/Makefile
  doc/reference/Makefile
  doc/reference/version.xml
])

# Generate files indicated by AC_CONFIG_FILES
AC_OUTPUT

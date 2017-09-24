# my versioning
m4_define([my_major], [0])
m4_define([my_minor], [8])
m4_define([my_micro], [2])
m4_define([my_version],	[my_major.my_minor.my_micro])

# Use -version option for libtool
m4_define([my_current], [m4_eval(my_minor * 10 + my_micro)])
m4_define([my_revision],[my_major])
m4_define([my_age],     [my_current])
m4_define([my_iface],	[my_current:my_revision:my_age])

# Autoconf 2.62 is bootstrapped with Automake 1.10.1.
# https://lists.gnu.org/archive/html/autotools-announce/2008-04/msg00002.html
AC_PREREQ(2.62)
# Initialize Autoconf
AC_INIT([libhinawa], [my_version], [o-takashi@sakamocchi.jp])
# The directory for helper scripts
AC_CONFIG_AUX_DIR([config])
# The directory for M4 macros
AC_CONFIG_MACRO_DIR([m4])

# The directory for sources
AC_CONFIG_SRCDIR([src])
# The header for variables with AC_DEFINE
AC_CONFIG_HEADERS([config.h])

# Automake 1.10 or later
# GObject Introspection uses GNU Make-specific functionality.
AM_INIT_AUTOMAKE([1.10.1  -Wno-portability])
# Don't output command lines
AM_SILENT_RULES([yes])

# GNU libtool 2.2.6 is bootstrapped with Automake 1.10.1 and Autoconf 2.62.
# https://lists.gnu.org/archive/html/autotools-announce/2008-09/msg00000.html
LT_PREREQ(2.2.6)
# Initialize GNU libtool
LT_INIT
# Define library version
LT_IFACE="my_iface"
AC_SUBST(LT_IFACE)

# Detect C language compiler
AC_PROG_CC

# Glib 2.34 or later
AM_PATH_GLIB_2_0([2.34.0], [], [], [gobject])

# GTK-Doc 1.18-2 or later
GTK_DOC_CHECK([1.18-2])

# GObject introspection 1.32.1 or later
GOBJECT_INTROSPECTION_REQUIRE([1.32.1])

# Check ALSA UAPI header for driver-dependent functionalities
with_snd_dice='no'
with_snd_efw='no'
with_snd_dg00x='no'
with_snd_motu='no'
AC_CHECK_FILE(
    [src/backport.h],
    [AC_DEFINE([WITH_BACKPORT], [1], [Define if backport header is available])
     with_snd_dice='yes'
     with_snd_efw='yes'
     with_snd_dg00x='yes'
     with_snd_motu='yes'],
    [AC_CHECK_TYPE([struct snd_firewire_event_dice_notification],
                   [with_snd_dice='yes'], [],
                   [#include <sound/firewire.h>])
     AC_CHECK_TYPE([struct snd_firewire_event_efw_response],
                   [with_snd_efw='yes'], [],
                   [#include <sound/firewire.h>])
     AC_CHECK_TYPE([struct snd_firewire_event_digi00x_message],
                   [with_snd_dg00x='yes'], [],
                   [#include <sound/firewire.h>])
     AC_CHECK_TYPE([struct snd_firewire_motu_status],
                   [with_snd_motu='yes'], [],
                   [#include <sound/firewire.h>])]
)
AM_CONDITIONAL(WITH_SND_DICE,  [test x$with_snd_dice = xyes])
AM_CONDITIONAL(WITH_SND_EFW,   [test x$with_snd_efw = xyes])
AM_CONDITIONAL(WITH_SND_DG00X, [test x$with_snd_dg00x = xyes])
AM_CONDITIONAL(WITH_SND_MOTU,  [test x$with_snd_motu = xyes])
AS_IF([test x$with_snd_dice = xyes],
      [AC_DEFINE([HAVE_SND_DICE], [1], [Define if snd_dice is available])])
AS_IF([test x$with_snd_efw = xyes],
      [AC_DEFINE([HAVE_SND_EFW], [1], [Define if snd_efw is available])])
AS_IF([test x$with_snd_dg00x = xyes],
      [AC_DEFINE([HAVE_SND_DG00X], [1], [Define if snd_dg00x is available])])
AS_IF([test x$with_snd_motu = xyes],
      [AC_DEFINE([HAVE_SND_MOTU], [1], [Define if snd_motu is available])])

# The files generated from *.in
AC_CONFIG_FILES([
  Makefile
  libhinawa.pc
  src/Makefile
  doc/Makefile
  doc/reference/Makefile
  doc/reference/version.xml
])

# Generate scripts and launch
AC_OUTPUT

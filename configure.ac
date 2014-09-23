AC_PREREQ(2.65)

AC_INIT([libhinawa], [0.3], [o-takashi@sakamocchi.jp])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_SRCDIR([src/internal.h])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
LT_INIT

# Checks for libraries.
AC_CHECK_LIB([asound], [snd_hwdep_open])
AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strerror])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  include/Makefile
])

AC_OUTPUT

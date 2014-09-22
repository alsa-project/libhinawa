CLEANFILES =

AM_CPPFLAGS =					\
	 -I$(top_builddir)			\
	 -I$(top_srcdir)

lib_LTLIBRARIES =				\
	libhinawa.la

libhinawa_la_LIBADD =				\
	-lasound -lpthread

libhinawa_la_SOURCES =				\
	reactor.c				\
	fw.c					\
	fcp.c					\
	snd.c					\
	efw.c					\
	internal.h

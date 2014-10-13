CLEANFILES =

AM_CPPFLAGS =					\
	 -I$(top_builddir)			\
	 -I$(top_srcdir)

AM_CFLAGS =					\
	$(GLIB_CFLAGS)				\
	$(GIO_CFLAGS)

lib_LTLIBRARIES =				\
	libghinawa.la

libghinawa_la_LIBADD =				\
	-lhinawa				\
	$(GLIB_LIBS)				\
	$(GIO_LIBS)

libghinawa_la_SOURCES =				\
	ghinawa.h				\
	ghinawa.c

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all
INTROSPECTION_COMPILER_ARGS =

if HAVE_INTROSPECTION
GHinawa-1.0.gir: libghinawa.la
GHinawa_1_0_gir_PACKAGES =
GHinawa_1_0_gir_EXPORT_PACKAGES = ghinawa
GHinawa_1_0_gir_INCLUDES = GObject-2.0
GHinawa_1_0_gir_CFLAGS =
GHinawa_1_0_gir_LIBS = libghinawa.la
GHinawa_1_0_gir_FILES = $(libghinawa_la_SOURCES)
GHinawa_1_0_gir_SCANNERFLAGS =			\
	--identifier-prefix=GHinawa		\
	--symbol-prefix=ghinawa			\
	-lhinawa
INTROSPECTION_GIRS += GHinawa-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES +=					\
	$(gir_DATA)				\
	$(typelib_DATA)
endif

CLEANFILES =

AM_CPPFLAGS =					\
	 -I$(top_builddir)			\
	 -I$(top_srcdir)

AM_CFLAGS =					\
	$(GLIB_CFLAGS)

lib_LTLIBRARIES =				\
	libhinawa.la

libhinawa_la_LIBADD =				\
	$(GLIB_LIBS)				\
	-lasound

libhinawa_la_SOURCES =				\
	fw_req.h				\
	fw_req.c				\
	fw_resp.h				\
	fw_resp.c				\
	fw_unit.h				\
	fw_unit.c				\
	snd_unit.h				\
	snd_unit.c

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all
INTROSPECTION_COMPILER_ARGS =

if HAVE_INTROSPECTION
Hinawa-1.0.gir: libhinawa.la
Hinawa_1_0_gir_PACKAGES =
Hinawa_1_0_gir_EXPORT_PACKAGES = hinawa
Hinawa_1_0_gir_INCLUDES = GObject-2.0
Hinawa_1_0_gir_CFLAGS =
Hinawa_1_0_gir_LIBS = libhinawa.la
Hinawa_1_0_gir_FILES = $(libhinawa_la_SOURCES)
Hinawa_1_0_gir_SCANNERFLAGS =			\
	--identifier-prefix=Hinawa		\
	--symbol-prefix=hinawa			\
	-lasound
INTROSPECTION_GIRS += Hinawa-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES +=					\
	$(gir_DATA)				\
	$(typelib_DATA)
endif

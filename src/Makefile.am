# Remove auto-generated files when cleaning
CLEANFILES =					\
	hinawa_sigs_marshal.*


AM_CPPFLAGS =					\
	-I$(top_builddir)			\
	-I$(top_srcdir)

AM_CFLAGS =					\
	$(GLIB_CFLAGS)				\
	-Wall

lib_LTLIBRARIES =				\
	libhinawa.la

libhinawa_la_LDFLAGS =				\
	-version-info $(LT_IFACE)		\
	-export-symbols $(top_srcdir)/src/hinawa.symbols

libhinawa_la_LIBADD =				\
	$(GLIB_LIBS)

BUILT_SOURCES =					\
	hinawa_sigs_marshal.h			\
	hinawa_sigs_marshal.c

SCANNED_SOURCES =				\
	$(BUILT_SOURCES)			\
	hinawa_context.h			\
	hinawa_context.c			\
	fw_unit.h				\
	fw_unit.c				\
	fw_resp.h				\
	fw_resp.c				\
	fw_req.h				\
	fw_req.c				\
	fw_fcp.h				\
	fw_fcp.c				\
	internal.h				\
	snd_unit.h				\
	snd_unit.c				\
	hinawa.symbols

pkginclude_HEADERS =				\
	fw_unit.h				\
	fw_resp.h				\
	fw_req.h				\
	fw_fcp.h				\
	snd_unit.h				\
	snd_dice.h				\
	snd_efw.h				\
	snd_dg00x.h				\
	snd_motu.h

if WITH_SND_DICE
SCANNED_SOURCES +=				\
	snd_dice.h				\
	snd_dice.c
endif

if WITH_SND_EFW
SCANNED_SOURCES +=				\
	snd_efw.h				\
	snd_efw.c
endif

if WITH_SND_DG00X
SCANNED_SOURCES +=				\
	snd_dg00x.h				\
	snd_dg00x.c
endif

if WITH_SND_MOTU
SCANNED_SOURCES +=				\
	snd_motu.h				\
	snd_motu.c
endif

libhinawa_la_SOURCES =				\
	$(SCANNED_SOURCES)			\
	backport.h

hinawa_sigs_marshal.list:
	$(AM_V_GEN)( find | grep \.c$$ | xargs cat | 			\
	sed -n -e 's/.*hinawa_sigs_marshal_\([A-Z]*__[A-Z_]*\).*/\1/p' |\
	sed -e 's/__/:/' -e 'y/_/,/' ) | sort -u > $@

hinawa_sigs_marshal.h: hinawa_sigs_marshal.list
	$(AM_V_GEN)( glib-genmarshal --header				\
		--prefix=hinawa_sigs_marshal $< ) > $@

hinawa_sigs_marshal.c: hinawa_sigs_marshal.list hinawa_sigs_marshal.h
	$(AM_V_GEN)(echo '#include "hinawa_sigs_marshal.h"';		\
		glib-genmarshal --body --prefix=hinawa_sigs_marshal $< ) > $@

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
Hinawa_1_0_gir_FILES = $(SCANNED_SOURCES)
Hinawa_1_0_gir_SCANNERFLAGS =			\
	--identifier-prefix=Hinawa		\
	--symbol-prefix=hinawa
INTROSPECTION_GIRS += Hinawa-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES +=					\
	$(gir_DATA)				\
	$(typelib_DATA)
endif

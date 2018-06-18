# Include m4 macros
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	meson.build				\
	meson_options.txt

SUBDIRS =					\
	src					\
	doc					\
	tests

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libhinawa.pc

#!/bin/sh

set -u
set -e

if [ ! -e m4 ] ; then
	mkdir -p m4
fi

gtkdocize --copy --docdir doc/reference
autoreconf --install

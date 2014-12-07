#!/bin/sh

set -u
set -e

mkdir -p m4

gtkdocize --copy --docdir doc/reference
autoreconf --install

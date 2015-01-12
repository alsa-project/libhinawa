#!/bin/bash

# NOTE:
# Current working directory should be in root of this repository.

export LD_LIBRARY_PATH=src/.libs/:/usr/lib:/lib
export GI_TYPELIB_PATH=src/:/usr/lib/girepository-1.0

if [[ $1 == 'qt4' ]] ; then
	./samples/qt4.py
elif [[ $1 == 'qt5' ]] ; then
	./samples/qt5.py
elif [[ $1 == 'gtk' ]] ; then
	./samples/gtk3.py
fi

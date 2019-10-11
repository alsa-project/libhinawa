#!/bin/bash

# NOTE:
# Current working directory should be in root of this repository.

# LD_LIBRARY_PATH and GI_TYPELIB_PATH environments variables should be se
# in advance.

if [[ $1 == 'qt5' ]] ; then
	./samples/qt5.py
elif [[ $1 == 'gtk' ]] ; then
	./samples/gtk3.py
fi

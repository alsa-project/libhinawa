#!/bin/bash

LD_LIBRARY_PATH=src/.libs/:/usr/lib:/lib GI_TYPELIB_PATH=src/:/usr/lib/girepository-1.0 python hinawa.py

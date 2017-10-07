#!/bin/sh

mkdir -p wxWidgets-2.8.12/build/gtk
cd wxWidgets-2.8.12/build/gtk
../../configure --prefix=${HOME}/apps --exec-prefix=${HOME}/apps --disable-shared --with-regex=builtin
make

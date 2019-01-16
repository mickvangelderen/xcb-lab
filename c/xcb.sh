#!/usr/bin/env sh

# https://www.x.org/releases/X11R7.7/doc/libxcb/tutorial/index.html
clang xcb.c -o xcb -lX11 -lxcb -Weverything
./xcb
rm xcb

#!/usr/bin/env sh

clang xcb-2.c -o xcb-2 -lX11 -lX11-xcb -lxcb -Weverything
./xcb-2
rm xcb-2


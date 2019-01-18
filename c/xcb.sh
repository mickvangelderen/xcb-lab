#!/usr/bin/env sh

# https://marc.info/?l=freedesktop-xcb&m=129381953404497
clang xcb.c -o xcb -lX11 -lxcb -Weverything
./xcb
rm xcb

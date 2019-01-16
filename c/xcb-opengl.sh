#!/usr/bin/env sh

clang xcb-opengl.c -o xcb-opengl -lX11 -lX11-xcb -lxcb -lGL -Weverything
./xcb-opengl
rm xcb-opengl


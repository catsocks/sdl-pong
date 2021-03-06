#!/usr/bin/env bash
dest=build
mkdir -p $dest
cc -o $dest/tennis \
	src/*.c \
	-std=c99 \
	-Wall -Wextra -pedantic \
	-lm \
	$(sdl2-config --cflags --libs) \
	"$@"

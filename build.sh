#!/bin/bash
mkdir -p build
cc -o build/tennis \
	src/*.c \
	-std=c99 \
	-Wall -Wextra -pedantic \
	-lm \
	$(sdl2-config --cflags --libs) \
	"$@"

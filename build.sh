#!/bin/bash

# -DCHEATS_ENABLED=1
# -DAUDIO_ENABLED=0
# -DCONTROLLER_ENABLED=0

dest=build
mkdir -p $dest
cc -o $dest/tennis \
	src/*.c \
	-std=c99 \
	-Wall -Wextra -pedantic \
	-lm \
	$(sdl2-config --cflags --libs) \
	"$@"

#!/usr/bin/env bash
dest=build
mkdir -p $dest
emcc -o $dest/game.html \
	src/*.c \
	--shell-file src/emscripten/shell.html \
	-std=c99 \
	-Wall -Wextra -pedantic \
	-s WASM=1 \
	-s USE_SDL=2 \
	"$@"

# SDL Pong

An attempt at recreating the [Pong](https://en.wikipedia.org/wiki/Pong) game
from 1972 (with sound) in C using only the [SDL](https://www.libsdl.org/)
library.

You can watch a recording of 1972 Pong here:
[Original Atari PONG (1972) arcade machine gameplay video - YouTube](https://www.youtube.com/watch?v=fiShX2pTz9A).

## Screenshot

![Screenshot](screenshot.png)

## Controls

* <kbd>W</kbd> and <kbd>S</kbd> moves the paddle on the left up and down
* <kbd>Up</kbd> and <kbd>Down</kbd> moves the paddle on the right up and down
* <kbd>F11</kbd> toggles desktop fullscreen mode

## Build

The only build requirements are a C compiler that supports C99 and the SDL
library (version 2 or higher). You can use CMake to generate build files or you
can simply run the build.sh script on a Unix-like system.

## License

Everything with the exception of the contents of the cmake folder is dedicated
to the public domain under the CC0 1.0 Universal license.

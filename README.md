# Tennis

An attempt to recreate the original [Pong](https://en.wikipedia.org/wiki/Pong)
game from 1972 with sound in C using only the [SDL](https://www.libsdl.org/)
cross-platform multimedia library.

The window is resizable, desktop fullscreen can be toggled with the keyboard and
up to 2 joysticks (incl. regular controllers) are supported.

You can watch a video of 1972 Pong on Youtube
[here](https://www.youtube.com/watch?v=fiShX2pTz9A).

## Screenshot

![Screenshot](screenshot.png)

## Controls

The vertical axis of up to two joysticks are used to control the movement of the
paddles, and they can be used simultaneously with the keyboard controls below.

* <kbd>W</kbd> and <kbd>S</kbd> moves the paddle on the left up and down
* <kbd>Up</kbd> and <kbd>Down</kbd> moves the paddle on the right up and down
* <kbd>F5</kbd> restarts the round
* <kbd>F11</kbd> toggles desktop fullscreen mode

## Build

The only build requirements are the SDL library version 2 and a C compiler with
support for C99. I have only built this project on Linux but it should be
buildable on Windows and macOS as it is or with very minor changes.

You can build the project using either [CMake](https://cmake.org/) or by simply
running build.sh on a Unix-like system.

## License

Everything with the exception of the contents of the cmake folder is dedicated
to the public domain under the CC0 1.0 Universal license.

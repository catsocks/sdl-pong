cc -Wall -Wextra -pedantic \
   -o pong \
   main.c \
   $(sdl2-config --cflags --libs) \
   -lm \
   "$@" # -DCHEATS

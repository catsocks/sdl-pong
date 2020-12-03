mkdir -p build
cc -o build/tennis \
	src/main.c src/math.c src/font.c src/tonegen.c \
	-std=c99 \
	-Wall -Wextra -pedantic \
	-lm \
	$(sdl2-config --cflags --libs) \
	"$@"

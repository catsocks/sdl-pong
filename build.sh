mkdir -p build
cc -o build/tennis \
	main.c \
	-std=c99 \
	$(sdl2-config --cflags --libs) \
	-lm \
	-Wall -Wextra -pedantic \
	"$@"

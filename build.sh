mkdir -p build
cc -o build/tennis \
	main.c \
	$(sdl2-config --cflags --libs) \
	-lm \
	-Wall -Wextra -pedantic \
	"$@"

CC := gcc
CF := -Wall -Wextra -pedantic
CI := $(shell pkg-config --cflags sdl3)
CL := $(shell pkg-config --libs sdl3)

OBJ := object/main.o object/chip8.o object/video.o object/audio.o object/input.o

.PHONY: all run clean

all: build/main

build/main: $(OBJ) | build/
	$(CC) $(CF) $(OBJ) -o build/main $(CL)

object/%.o: src/%.c | object/
	$(CC) $(CF) -MMD -MP -c $< -o $@ -Iinclude $(CI)

-include $(OBJ:.o=.d)

run: build/main
	./build/main $(ARGS)

clean:
	rm -rf build/ dobject/

build/:
	mkdir -p build/

object/:
	mkdir -p object/
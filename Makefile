CC := gcc
CF := -Wall -Wextra
CI := $(shell pkg-config --cflags sdl3)
CL := $(shell pkg-config --libs sdl3)

OBJ := object/main.o object/chip8.o

.PHONY: all run debug clean

all: build/main

build/main: $(OBJ) | build/
	$(CC) $(CF) $(OBJ) -o build/main $(CL)

object/%.o: src/%.c | object/
	$(CC) $(CF) -MMD -MP -c $< -o $@ -Iinclude $(CI)

-include $(OBJ:.o=.d)

debug: $(OBJ) | debugbuild/
	$(CC) -g -O0 -fsanitize=address $(CF) $(OBJ) -o debugbuild/main $(CL)

run: build/main
	./build/main $(ARGS)

clean:
	rm -rf build/ debugbuild/ object/

build/:
	mkdir -p build/

object/:
	mkdir -p object/

debugbuild/:
	mkdir -p debugbuild/
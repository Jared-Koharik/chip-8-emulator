CC := gcc
CF := `pkg-config --cflags --libs sdl3`

BUILD_NAME := main
BUILD_PATH := build

all: src/main.c | build
	$(CC) src/main.c -o $(BUILD_PATH)/$(BUILD_NAME) $(CF)

$(BUILD_PATH)/$(BUILD_NAME):
	$(MAKE) all

build:
	mkdir -p build

run: $(BUILD_PATH)/$(BUILD_NAME)
	./$(BUILD_PATH)/$(BUILD_NAME)

buildDebug: src/main.c | debug
	$(CC) -g -O0 -fsanitize=address src/main.c -o debug/main $(CF)

debug:
	mkdie -p debug

clean:
	rm -r $(BUILD_PATH)/$(BUILD_NAME)

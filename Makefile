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

clean:
	rm -r $(BUILD_PATH)/$(BUILD_NAME)

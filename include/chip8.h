#ifndef CHIP8_H
#define CHIP8_H

#include <stdbool.h>
#include <stdint.h>

// Defining the dimensions of the Chip8 screen
#define PIXEL_WIDTH 64
#define PIXEL_HEIGHT 32

#define STACK_SIZE 16
#define MEMORY_SIZE 4096
#define REGISTER_SIZE 16
#define KEYPAD_SIZE 16

typedef struct Chip8{

  uint32_t screenPixels[PIXEL_WIDTH * PIXEL_HEIGHT];

  uint16_t stack[STACK_SIZE];
  uint16_t addressRegister;
  uint16_t programCounter;

  uint8_t memory[MEMORY_SIZE];
  uint8_t generalRegisters[REGISTER_SIZE];
  uint8_t stackPointer;
  uint8_t delayTimer;
  uint8_t soundTimer;
  uint8_t pressedKey;

  bool keypad[KEYPAD_SIZE];
  bool keyIsPressed;

  bool isClassic;
  bool readyToRender;

} Chip8;

bool initChip8(Chip8 *restrict pchip8, bool isClassic, const char *restrict romToLoad);
bool loadROM(Chip8 *restrict pchip8, const char *restrict pfilePath);

uint32_t *getPixels(Chip8 *restrict pchip8);

bool checkDrawFlag(Chip8 *restrict pchip8);
bool checkSoundTimer(Chip8 *restrict pchip8);
bool checkStall(Chip8 *restrict pchip8);

bool tickTimers(Chip8 *restrict pchip8);
bool executeNextInstruction(Chip8 *restrict pchip8);

#endif
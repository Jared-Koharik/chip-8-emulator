#ifndef CHIP8_H
#define CHIP8_H

#include <stdbool.h>
#include <stdint.h>

typedef struct{

  uint32_t screenPixels[64 * 32];

  uint16_t stack[16];
  uint16_t addressRegister;
  uint16_t programCounter;
  uint16_t opcode;

  uint8_t memory[4096];
  uint8_t generalRegisters[16];
  uint8_t stackPointer;
  uint8_t delayTimer;
  uint8_t soundTimer;
  uint8_t pressedKey;

  bool keypad[16];
  bool keyIsPressed;

  bool isClassic;
  bool readyToRender;

} Chip8;

bool initChip8(Chip8 *restrict pchip8, bool isClassic, const char *restrict romToLoad);
bool loadROM(Chip8 *restrict pchip8, const char *restrict pfilePath);

bool executeNextInstruction(Chip8 *restrict pchip8);

#endif
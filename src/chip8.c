#include "chip8.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Defining values of different parts of memory in the Chip8
// The Chip8 system has a total of 4096 bytes of memory with some reserved sections
#define RESERVED_MEMORY_FOR_INTERPRETER 512
#define RESERVED_MEMORY_FOR_INTERNAL 96
#define RESERVED_MEMORY_FOR_REFRESH 256
#define AVAILABLE_MEMORY (MEMORY_SIZE - RESERVED_MEMORY_FOR_INTERPRETER - RESERVED_MEMORY_FOR_INTERNAL - RESERVED_MEMORY_FOR_REFRESH)
#define CHARACTERS_ARRAY_SIZE 80
#define FONT_ADDRESS 0x50
#define PROGRAM_START_ADDRESS 0x200
#define PROGRAM_MAX_ADDRESS 0xE9F

#define GET_BIT(byte, bit) (byte & ( 0x80u >> bit ))
#define GET_FAMILY(opcode) (opcode & 0xF000) >> 0xC
#define GET_VX(opcode) ((opcode & 0x0F00) >> 0x8)
#define GET_VY(opcode) ((opcode & 0x00F0) >> 0x4)
#define GET_N(opcode) (opcode & 0x000F)
#define GET_NN(opcode) (opcode & 0x00FF)
#define GET_NNN(opcode) (opcode & 0x0FFF)

static bool exeIntrucFamily0(Chip8 *restrict pchip8, uint16_t opcode) {

  switch(opcode) {
    case 0xE0:
      memset(pchip8->screenPixels, 0, sizeof((pchip8->screenPixels)));
      pchip8->readyToRender = true;
      break;
    case 0xEE:
      if( pchip8->stackPointer != 0) {
        pchip8->stackPointer--;
        pchip8->programCounter = pchip8->stack[pchip8->stackPointer];
      }
      break;
    default:
      return false;
      break;
  }

  return true;

}
static bool exeIntrucFamily1(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint16_t NNN = GET_NNN(opcode);

  pchip8->programCounter = NNN;

  return true;

}
static bool exeIntrucFamily2(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint16_t NNN = GET_NNN(opcode);

  if( pchip8->stackPointer < 16) {
    pchip8->stack[pchip8->stackPointer] = pchip8->programCounter;
    pchip8->stackPointer++;
    pchip8->programCounter = NNN;
  } else {
    return false;
  }

  return true;

}
static bool exeIntrucFamily3(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  if( pchip8->generalRegisters[Vx] == NN ) pchip8->programCounter += 2;

  return true;

}
static bool exeIntrucFamily4(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  if( pchip8->generalRegisters[Vx] != NN ) pchip8->programCounter += 2;

  return true;

}
static bool exeIntrucFamily5(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t Vy = GET_VY(opcode);

  if( pchip8->generalRegisters[Vx] == pchip8->generalRegisters[Vy] ) pchip8->programCounter += 2;
  
  return true;

}
static bool exeIntrucFamily6(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  pchip8->generalRegisters[Vx] = NN;

  return true;

}
static bool exeIntrucFamily7(Chip8 *restrict pchip8, uint16_t opcode) {
  
  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  pchip8->generalRegisters[Vx] += NN;

  return true;

}
static bool exeIntrucFamily8(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t Vy = GET_VY(opcode);
  const uint8_t N = GET_N(opcode);

  uint8_t prevValue;

  uint8_t *reg = pchip8->generalRegisters;

  switch(N) {
    case 0x0:
      reg[Vx] = reg[Vy];
      break;
    case 0x1: // Ambiguous
      if(pchip8->isClassic) reg[0xF] = 0;
      reg[Vx] |= reg[Vy];
      break;
    case 0x2: // Ambiguous
      if(pchip8->isClassic) reg[0xF] = 0;
      reg[Vx] &= reg[Vy];
      break;
    case 0x3: // Ambiguous
      if(pchip8->isClassic) reg[0xF] = 0;
      reg[Vx] ^= reg[Vy];
      break;
    case 0x4:
      prevValue = reg[Vx];
      reg[Vx] += reg[Vy];
      reg[0xF] = reg[Vx] < prevValue;
      break;
    case 0x5:
      prevValue = reg[Vx];
      reg[Vx] -= reg[Vy];
      reg[0xF] = reg[Vx] <= prevValue;
      break;
    case 0x6: // Ambiguous
      if(pchip8->isClassic) reg[Vx] = reg[Vy];
      prevValue = reg[Vx] & 0x1;
      reg[Vx] >>= 1;
      reg[0xF] = prevValue;
      break;
    case 0x7:
      prevValue = reg[Vy];
      reg[Vx] = reg[Vy] - reg[Vx];
      reg[0xF] = reg[Vx] <= prevValue;
      break;
    case 0xE: // Ambiguous
      if(pchip8->isClassic) reg[Vx] = reg[Vy];
      prevValue = (reg[Vx] & 0x80) >> 0x7;
      reg[Vx] <<= 1;
      reg[0xF] = prevValue;
      break;
    default:
      return false;
      break;
  }

  return true;

}
static bool exeIntrucFamily9(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t Vy = GET_VY(opcode);

  if( pchip8->generalRegisters[Vx] != pchip8->generalRegisters[Vy] ) pchip8->programCounter += 2;

  return true;

}
static bool exeIntrucFamilyA(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint16_t NNN = GET_NNN(opcode);

  pchip8->addressRegister = NNN;

  return true;

}
static bool exeIntrucFamilyB(Chip8 *restrict pchip8, uint16_t opcode) {

  // Is an ambiguous instruction, but according to Tvil it is most likely fine to leave it like this:
  //   https://tobiasvl.github.io/blog/write-a-chip-8-emulator/#bnnn-jump-with-offset
  const uint16_t NNN = GET_NNN(opcode);

  pchip8->programCounter = NNN + pchip8->generalRegisters[0];

  return true;

}
static bool exeIntrucFamilyC(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  pchip8->generalRegisters[Vx] = (rand() % 256) & NN;

  return true;

}
static bool exeIntrucFamilyD(Chip8 *restrict pchip8, uint16_t opcode) {

  // When drawing sprites, the initial drawing location should wrap around the screen
  // While drawing however, the sprites should not wrap
  // https://tobiasvl.github.io/blog/write-a-chip-8-emulator/#dxyn-display

  uint32_t *restrict pixels = pchip8->screenPixels;
  uint8_t *restrict reg = pchip8->generalRegisters;

  const uint8_t *restrict mem = pchip8->memory;

  const uint16_t address = pchip8->addressRegister;

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t Vy = GET_VY(opcode);
  const uint8_t N = GET_N(opcode);

  const uint8_t xMax = PIXEL_WIDTH;
  const uint8_t yMax = PIXEL_HEIGHT;

  // Initial location is wrapped around the screen using modulo if it goes over either direction
  const uint8_t xPos = reg[Vx] % xMax;
  const uint8_t yPos = reg[Vy] % yMax;

  reg[0xF] = 0x0;

  for(uint8_t row = 0; row < N; row++) {

    const uint8_t spriteByte = mem[address + row];

    for(uint8_t column = 0; column < 8; column++) {

      // While drawing, the sprite DOES NOT wrap, this checks for that
      if((xPos + column) < xMax && (yPos + row) < yMax){

        const uint8_t spritePixel = GET_BIT(spriteByte, column);

        if(spritePixel > 0) {

          const uint16_t pixelIndex = (xPos + column) + PIXEL_WIDTH * (yPos + row);

          if(pixels[pixelIndex] > 0) {
            reg[0xF] = 1;
          }

          pixels[pixelIndex] ^= 0xFFFFFFFF;

        }

      }      

    }
  }

  pchip8->readyToRender = true;

  return true;
}
static bool exeIntrucFamilyE(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint8_t Vx = GET_VX(opcode);

  switch(opcode & 0x00FF) {
    case 0x9E:
      if((pchip8->keypad[ pchip8->generalRegisters[Vx] & 0xF])) pchip8->programCounter += 2;
      break;
    case 0xA1:
      if(!((pchip8->keypad[ pchip8->generalRegisters[Vx] & 0xF]))) pchip8->programCounter += 2;
      break;
    default:
      return false;
      break;
  }

  return true;
}
static bool exeIntrucFamilyF(Chip8 *restrict pchip8, uint16_t opcode) {

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  switch(NN) {
    case 0x07:
      pchip8->generalRegisters[Vx] = pchip8->delayTimer;
      break;
    case 0x0A:
      if(pchip8->keyIsPressed) {
        if((pchip8->keypad[pchip8->pressedKey])) {
          pchip8->programCounter -= 2;
        } else {
          pchip8->keyIsPressed = false;
          pchip8->generalRegisters[Vx] = pchip8->pressedKey;
        }
      } else {
        for(uint8_t i = 0; i <= 0xf; i++) {
          if((pchip8->keypad[i])) {
            pchip8->keyIsPressed = true;
            pchip8->pressedKey = i;
            break;
          }
        }
        pchip8->programCounter -= 2;
      }
      break;
    case 0x15:
      pchip8->soundTimer = pchip8->generalRegisters[Vx];
      break;
    case 0x18:
      pchip8->delayTimer = pchip8->generalRegisters[Vx];
      break;
    case 0x1E:
      pchip8->addressRegister += pchip8->generalRegisters[Vx];
      break;
    case 0x29:
      pchip8->addressRegister = FONT_ADDRESS + (pchip8->generalRegisters[Vx] * 5);
      break;
    case 0x33:
      pchip8->memory[pchip8->addressRegister] = pchip8->generalRegisters[Vx] / 100;
      pchip8->memory[pchip8->addressRegister + 1] = (pchip8->generalRegisters[Vx] / 10) % 10;
      pchip8->memory[pchip8->addressRegister + 2] = pchip8->generalRegisters[Vx] % 10;
      break;
    case 0x55:
      for(uint8_t i = 0; i <= Vx; i++) {
        pchip8->memory[pchip8->addressRegister + i] = pchip8->generalRegisters[i];
      }
      if(pchip8->isClassic) pchip8->addressRegister += Vx + 1;
      break;
    case 0x65:
      for(uint8_t i = 0; i <= Vx; i++) {
        pchip8->generalRegisters[i] = pchip8->memory[pchip8->addressRegister + i];
      }
      if(pchip8->isClassic) pchip8->addressRegister += Vx + 1;
      break;
    default:
      return false;
      break;
  }

  return true;

}

typedef bool (*FamilyFunction)(Chip8 *restrict pchip8, uint16_t opcode);

static const FamilyFunction familyFunctions[0x10] = {
    exeIntrucFamily0,
    exeIntrucFamily1,
    exeIntrucFamily2,
    exeIntrucFamily3,
    exeIntrucFamily4,
    exeIntrucFamily5,
    exeIntrucFamily6,
    exeIntrucFamily7,
    exeIntrucFamily8,
    exeIntrucFamily9,
    exeIntrucFamilyA,
    exeIntrucFamilyB,
    exeIntrucFamilyC,
    exeIntrucFamilyD,
    exeIntrucFamilyE,
    exeIntrucFamilyF,
};

bool initChip8(Chip8 *restrict pchip8, bool isClassic, const char *restrict romToLoad) {

  pchip8->programCounter = PROGRAM_START_ADDRESS;

  pchip8->isClassic = isClassic;

  const uint8_t characters[CHARACTERS_ARRAY_SIZE] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
  };

  for(uint8_t i = 0; i < CHARACTERS_ARRAY_SIZE; i++) {
    pchip8->memory[FONT_ADDRESS + i] = characters[i];
  }

  if( !loadROM(pchip8, romToLoad) ) return false;

  srand(time(NULL));

  return true;

}

bool loadROM(Chip8 *restrict pchip8, const char *restrict pfilePath) {

  FILE *pROM = fopen(pfilePath, "rb");

  if( pROM == NULL ) {
    perror("fopen() failed to open ROM");
    return false;
  }

  size_t numRead = fread(pchip8->memory + PROGRAM_START_ADDRESS, sizeof(uint8_t), AVAILABLE_MEMORY, pROM);
  if( feof(pROM) ) {
    printf("Read %ld bytes from %s\n", numRead, pfilePath);
  } else if( ferror(pROM) ) {
    perror("Error reading from rom file");
    fclose(pROM);
    return false;
  }

  fclose(pROM);

  return true;

}

uint32_t *getPixels(Chip8 *restrict pchip8) {
  return pchip8->screenPixels;
}

bool checkDrawFlag(Chip8 *restrict pchip8) {

  if(pchip8->readyToRender) {
    pchip8->readyToRender = false;
    return true;
  }

  return false;

}

bool checkSoundTimer(Chip8 *restrict pchip8) {

  return pchip8->soundTimer > 0;

}

bool tickTimers(Chip8 *restrict pchip8) {

    if(pchip8->soundTimer > 0) pchip8->soundTimer--;
    if(pchip8->delayTimer > 0) pchip8->delayTimer--;

    return true;

}

bool executeNextInstruction(Chip8 *restrict pchip8) {

  if(pchip8->programCounter < 0x200 || pchip8->programCounter > (PROGRAM_MAX_ADDRESS - 1)) return false;

  uint16_t opcode = 0x0;
  opcode |= pchip8->memory[pchip8->programCounter] << 0x8;
  pchip8->programCounter++;
  opcode |= pchip8->memory[pchip8->programCounter];
  pchip8->programCounter++;

  const uint8_t family = GET_FAMILY(opcode);

  return familyFunctions[family](pchip8, opcode);

}
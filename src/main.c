#include <stdlib.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define LOGICAL_WIDTH 64
#define LOGICAL_HEIGHT 32

#define FILE_TO_LOAD "test-suite-files/1-chip8-logo.ch8"

#define MEMORY_SIZE 4096
#define RESERVED_MEMORY_FOR_INTERPRETER 512
#define RESERVED_MEMORY_FOR_INTERNAL 96
#define RESERVED_MEMORY_FOR_REFRESH 256
#define AVAILABLE_MEMORY (MEMORY_SIZE - RESERVED_MEMORY_FOR_INTERPRETER - RESERVED_MEMORY_FOR_INTERNAL - RESERVED_MEMORY_FOR_REFRESH)

#define FONT_SIZE 80
#define FONT_ADDRESS 0x50

#define PROGRAM_START 0x200

typedef struct Chip8 {

  uint16_t stack[16];
  uint16_t addressRegister;
  uint16_t programCounter;
  uint16_t opcode;

  uint8_t memory[MEMORY_SIZE];
  uint8_t screenPixels[LOGICAL_WIDTH * LOGICAL_HEIGHT];
  uint8_t generalRegisters[16];
  uint8_t keypad[16];
  uint8_t stackPointer;
  uint8_t delayTimer;
  uint8_t soundTimer;

} Chip8;

typedef struct AppContext {
  SDL_Window* pwindow;
  SDL_Renderer* prenderer;
  uint width, height;
} AppContext;

static bool setup(AppContext *restrict pcontext, Chip8 *restrict pchip8);

static bool initContext(AppContext *restrict pcontext);
static bool initChip8(Chip8 *restrict pchip8);

static void close(AppContext *restrict pcontext, Chip8 *restrict chip8);

static bool executeNextInstruction(Chip8 *restrict chip8);

int main(void) {

  int quit = false;

  AppContext context = { 0 };
  Chip8 chip8 = { 0 };

  if( !setup(&context, &chip8) ) {
    close(&context, &chip8);
    return EXIT_FAILURE;
  }

  while(!quit) {

    SDL_Event event;

    while(SDL_PollEvent(&event)) {

      switch(event.type) {

        case SDL_EVENT_QUIT:
          quit = true;
          break;

        default:
          break;

      }

    }

    SDL_RenderPresent(context.prenderer);

  }

  close(&context, &chip8);

  return EXIT_SUCCESS;
}

static bool setup(AppContext *restrict context, Chip8 *restrict chip8) {

  if( !SDL_SetAppMetadata("Chip 8 Emulator", "1.0", NULL) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set meta data: %s", SDL_GetError());
    return false;
  }

  if( !SDL_InitSubSystem(SDL_INIT_VIDEO) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to init video sub system: %s", SDL_GetError());
    return false;
  }

  if( !initContext(context) ) {
    return false;
  }

  if( !initChip8(chip8) ) {
    return false;
  }

  return true;

}

static bool initContext(AppContext *restrict context) {

  context->width = LOGICAL_WIDTH * 10;
  context->height = LOGICAL_HEIGHT * 10;

  if( !SDL_CreateWindowAndRenderer("Chip 8 Emulator", context->width, context->height, 0x0, &(context->pwindow), &(context->prenderer)) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to create window: %s", SDL_GetError());
    return false;
  }

  if( !SDL_SetRenderLogicalPresentation(context->prenderer, LOGICAL_WIDTH, LOGICAL_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set logical representation to letterbox: %s", SDL_GetError());
    return false;
  }

}

static bool initChip8(Chip8 *restrict chip8) {

  chip8->programCounter = PROGRAM_START;

  // Add the characters into memory for ROMs to use
  const uint8_t characters[FONT_SIZE] = {
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

  for(uint i = 0; i < FONT_SIZE; i++) {
    chip8->memory[FONT_ADDRESS + i] = characters[i];
  }

  return true;

}

static void close(AppContext *restrict context, Chip8 *restrict chip8) {

  SDL_DestroyRenderer(context->prenderer);
  SDL_DestroyWindow(context->pwindow);

}

static bool executeNextInstruction(Chip8 *restrict chip8) {

  switch(0x0) {
    case 0x0:
      break;
    case 0x1:
      break;
    case 0x2:
      break;
    case 0x3:
      break;
    case 0x4:
      break;
    case 0x5:
      break;
    case 0x6:
      break;
    case 0x7:
      break;
    case 0x8:
      break;
    case 0x9:
      break;
    case 0xA:
      break;
    case 0xB:
      break;
    case 0xC:
      break;
    case 0xD:
      break;
    case 0xE:
      break;
    case 0xF:
      break;
    default:
      break;
  }

  return true;

}
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// Defining the dimensions of the Chip8 screen
#define LOGICAL_WIDTH 64
#define LOGICAL_HEIGHT 32

// Defining values of different parts of memory in the Chip8
  // The Chip8 system has a total of 4096 bytes of memory with some reserved sections
  #define MEMORY_SIZE 4096
  #define RESERVED_MEMORY_FOR_INTERPRETER 512
  #define RESERVED_MEMORY_FOR_INTERNAL 96
  #define RESERVED_MEMORY_FOR_REFRESH 256
  #define AVAILABLE_MEMORY (MEMORY_SIZE - RESERVED_MEMORY_FOR_INTERPRETER - RESERVED_MEMORY_FOR_INTERNAL - RESERVED_MEMORY_FOR_REFRESH)
  #define FONT_SIZE 80
  #define FONT_ADDRESS 0x50
  #define PROGRAM_START 0x200

// Defining how many times per second certain actions are taken
  // Timers, Rendering, and Instruction execution
    // We want the timer to decrement 60 times a second per the Chip8 specification
    #define TIMER_FREQ_S 60.0f
    // This allows for specifying how often the screen is redrawn
    #define RENDER_FREQ_S 60.0f
    // This allows for specifying how fast we want the Chip8 ROM to run
    #define INSTRUCTION_FREQ_S 500.0f

    #define MS_PER_TIMER_DECREMENT (1000.0 / TIMER_FREQ_S)
    #define MS_PER_RENDER_PASS (1000.0 / RENDER_FREQ_S)
    #define MS_PER_INSTRUCTION_EXECUTE (1000.0 / INSTRUCTION_FREQ_S)

// Defining the square wave that will play when the sound timer is non-zero
#define WAVE_AMPLITUDE 0.1f  
#define WAVE_CYCLE_PER_S 110.0f
#define SAMPLE_PER_S 44100.0f
#define SOUND_LENGTH_S 0.5f

#define CYCLES_PER_

// Allows specifying the rom file that will be loaded
#define ROM_FILE "test-roms/6-keypad.ch8"

typedef struct Chip8 {

  uint32_t screenPixels[LOGICAL_WIDTH * LOGICAL_HEIGHT];

  uint16_t stack[16];
  uint16_t addressRegister;
  uint16_t programCounter;
  uint16_t opcode;

  uint8_t memory[MEMORY_SIZE];
  uint8_t generalRegisters[16];
  uint8_t stackPointer;
  uint8_t delayTimer;
  uint8_t soundTimer;
  uint8_t pressedKey;

  const bool * keypad[16];
  bool keyIsPressed;

} Chip8;

typedef struct AppContext {
  SDL_Window *pwindow;
  SDL_Renderer *prenderer;
  SDL_Texture *ptexture;
  SDL_AudioStream *stream;
  SDL_AudioSpec inputSpec;
  SDL_AudioDeviceID deviceID;
  uint width, height;
  bool quit;
} AppContext;

static bool startApp(AppContext *restrict pcontext, Chip8 *restrict pchip8);

static bool initContext(AppContext *restrict pcontext);
static bool initChip8(Chip8 *restrict pchip8);

static void handleSDLEvents(AppContext *restrict pcontext);

static bool loadROM(Chip8 *restrict pchip8, const char *restrict pfilePath);
static void opcodeDXYN(Chip8 *restrict pchip8, uint16_t opcode);
static void opcode8XYN(Chip8 *restrict pchip8, uint16_t opcode);
static void opcodeFXNN(Chip8 *restrict pchip8, uint16_t opcode);
static void opcodeFX0A(Chip8 *restrict pchip8, uint16_t opcode);

static bool executeNextInstruction(AppContext *restrict pcontext, Chip8 *restrict pchip8);

static void quitApp(AppContext *restrict pcontext, Chip8 *restrict pchip8);

int main(int argc, char *argv[]) {

  double instructionCounter = 0.0;
  double timerCounter = 0.0;
  double renderCounter = 0.0;

  int numSamples = (int)(SOUND_LENGTH_S * SAMPLE_PER_S);
  double numCycles = SOUND_LENGTH_S * WAVE_CYCLE_PER_S;

  double numSamplesPerCycle = numSamples / numCycles;

  double secondsPerCycle = 1.0f / WAVE_CYCLE_PER_S;

  float buff[numSamples];

  Uint64 nowTime, prevTime, deltaTime = 0;

  AppContext context = { 0 };
  Chip8 chip8 = { 0 };

  if( !startApp(&context, &chip8) ) {
    quitApp(&context, &chip8);
    return EXIT_FAILURE;
  }

  double amplitude = WAVE_AMPLITUDE;
  for (int sample = 0; sample < numSamples; sample++) {
    if( sample % (int)(numSamplesPerCycle / 2.0f) == 0) amplitude *= -1;
    buff[sample] = amplitude;
  }

  prevTime = SDL_GetTicks();

  while(!context.quit) {

    handleSDLEvents(&context);

    nowTime = SDL_GetTicks();
    double deltaTime = (double)(nowTime - prevTime);
    prevTime = nowTime;

    instructionCounter += deltaTime;
    timerCounter += deltaTime;
    renderCounter += deltaTime;

    while( instructionCounter >= MS_PER_INSTRUCTION_EXECUTE) {
      executeNextInstruction(&context, &chip8);
      instructionCounter -= MS_PER_INSTRUCTION_EXECUTE;
    }

    while( timerCounter >= MS_PER_TIMER_DECREMENT ) {
      if(chip8.soundTimer > 0) chip8.soundTimer--;
      if(chip8.delayTimer > 0) chip8.delayTimer--;
      timerCounter -= MS_PER_TIMER_DECREMENT;
    }

    if( renderCounter >= MS_PER_RENDER_PASS ) {
      SDL_UpdateTexture(context.ptexture, NULL, chip8.screenPixels, sizeof(chip8.screenPixels[0]) * LOGICAL_WIDTH);
      SDL_RenderClear(context.prenderer);
      SDL_RenderTexture(context.prenderer, context.ptexture, NULL, NULL);
      SDL_RenderPresent(context.prenderer);
      renderCounter = SDL_fmod(renderCounter, MS_PER_RENDER_PASS);
    }

    if(chip8.soundTimer > 0) {
      if(SDL_GetAudioStreamQueued(context.stream) == 0) {
        SDL_PutAudioStreamData(context.stream, buff, sizeof(buff));
      }
    } else {
      SDL_ClearAudioStream(context.stream);
    }

    SDL_Delay(1);
  }

  quitApp(&context, &chip8);

  return EXIT_SUCCESS;
}

static bool startApp(AppContext *restrict pcontext, Chip8 *restrict pchip8) {

  SDL_AudioSpec spec;

  if( !SDL_SetAppMetadata("Chip 8 Emulator", "1.0", NULL) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set meta data: %s", SDL_GetError());
    return false;
  }

  if( !SDL_InitSubSystem(SDL_INIT_VIDEO |  SDL_INIT_AUDIO ) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to init video sub system: %s", SDL_GetError());
    return false;
  }

  if( !initContext(pcontext) ) {
    return false;
  }

  if( !initChip8(pchip8) ) {
    return false;
  }

  return true;

}

static bool initContext(AppContext *restrict pcontext) {

  pcontext->width = LOGICAL_WIDTH * 10;
  pcontext->height = LOGICAL_HEIGHT * 10;

  if( !SDL_CreateWindowAndRenderer("Chip 8 Emulator", pcontext->width, pcontext->height, 0x0, &(pcontext->pwindow), &(pcontext->prenderer)) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to create window: %s", SDL_GetError());
    return false;
  }

  SDL_SetRenderDrawColor(pcontext->prenderer, 0, 0, 0, 255);

  pcontext->ptexture = SDL_CreateTexture(pcontext->prenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, LOGICAL_WIDTH, LOGICAL_HEIGHT);
  if( pcontext->ptexture == NULL ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s", SDL_GetError());
    return false;
  }
  SDL_SetTextureScaleMode(pcontext->ptexture, SDL_SCALEMODE_NEAREST);
  if( !SDL_SetRenderLogicalPresentation(pcontext->prenderer, LOGICAL_WIDTH, LOGICAL_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set logical representation to letterbox: %s", SDL_GetError());
    return false;
  }

  pcontext->deviceID = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
  if( pcontext->deviceID == 0 ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio device: %s", SDL_GetError());
    return false;
  }

  SDL_GetAudioDeviceFormat(pcontext->deviceID, &(pcontext->inputSpec), NULL);

  pcontext->inputSpec.format = SDL_AUDIO_F32;
  pcontext->inputSpec.channels = 1;
  pcontext->inputSpec.freq = SAMPLE_PER_S;

  pcontext->stream = SDL_CreateAudioStream(&(pcontext->inputSpec), NULL);
  if( pcontext->stream == NULL ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create audio stream: %s", SDL_GetError());
    return false;
  }

  if( !SDL_BindAudioStream(pcontext->deviceID, pcontext->stream) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to bind audio stream to device: %s", SDL_GetError());
    return false;
  }

  if( !SDL_ResumeAudioStreamDevice(pcontext->stream) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to resume audio stream device: %s", SDL_GetError());
    return false;
  }

  return true;

}

static bool initChip8(Chip8 *restrict pchip8) {

  pchip8->programCounter = PROGRAM_START;

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
    pchip8->memory[FONT_ADDRESS + i] = characters[i];
  }

  int numKeys;
  const bool *sdlKeys = SDL_GetKeyboardState(&numKeys);

  pchip8->keypad[0x0] = &(sdlKeys[SDL_SCANCODE_X]);
  pchip8->keypad[0x1] = &(sdlKeys[SDL_SCANCODE_1]);
  pchip8->keypad[0x2] = &(sdlKeys[SDL_SCANCODE_2]);
  pchip8->keypad[0x3] = &(sdlKeys[SDL_SCANCODE_3]);
  pchip8->keypad[0x4] = &(sdlKeys[SDL_SCANCODE_Q]);
  pchip8->keypad[0x5] = &(sdlKeys[SDL_SCANCODE_W]);
  pchip8->keypad[0x6] = &(sdlKeys[SDL_SCANCODE_E]);
  pchip8->keypad[0x7] = &(sdlKeys[SDL_SCANCODE_A]);
  pchip8->keypad[0x8] = &(sdlKeys[SDL_SCANCODE_S]);
  pchip8->keypad[0x9] = &(sdlKeys[SDL_SCANCODE_D]);
  pchip8->keypad[0xA] = &(sdlKeys[SDL_SCANCODE_Z]);
  pchip8->keypad[0xB] = &(sdlKeys[SDL_SCANCODE_C]);
  pchip8->keypad[0xC] = &(sdlKeys[SDL_SCANCODE_4]);
  pchip8->keypad[0xD] = &(sdlKeys[SDL_SCANCODE_R]);
  pchip8->keypad[0xE] = &(sdlKeys[SDL_SCANCODE_F]);
  pchip8->keypad[0xF] = &(sdlKeys[SDL_SCANCODE_V]);

  if( !loadROM(pchip8, ROM_FILE) ) return false;

  return true;

}

static void handleSDLEvents(AppContext *restrict pcontext) {
  SDL_Event event;

  while(SDL_PollEvent(&event)) {

    switch(event.type) {

      case SDL_EVENT_QUIT:
        pcontext->quit = true;
        break;

      default:
        break;

    }

  }
}

static bool loadROM(Chip8 *restrict pchip8, const char *restrict pfilePath) {

  FILE *pROM = fopen(pfilePath, "rb");
  if( pROM == NULL ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Can not open file %s", pfilePath);
    return false;
  }

  size_t numRead = fread(pchip8->memory + PROGRAM_START, sizeof(uint8_t), AVAILABLE_MEMORY, pROM);
  if( ferror(pROM) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Can not read from file %s", pfilePath);
    fclose(pROM);
    return false;
  } else if (feof(pROM)) {
    SDL_Log("Succesful full read from file: %s\n Read count: %lu", pfilePath, numRead);
  } else {
    SDL_Log("Succesful partial read from file: %s\n Read count: %lu", pfilePath, numRead);
  }

  fclose(pROM);

  return true;

}

static void opcodeDXYN(Chip8 *restrict pchip8, uint16_t opcode) {

  pchip8->generalRegisters[0xF] = 0x0;

  uint8_t Vx = (opcode & 0x0F00) >> 0x8;
  uint8_t Vy = (opcode & 0x00F0) >> 0x4;
  uint8_t n = opcode & 0x000F;

  uint8_t xPos = pchip8->generalRegisters[Vx] % LOGICAL_WIDTH;
  uint8_t yPos = pchip8->generalRegisters[Vy] % LOGICAL_HEIGHT;

  for(uint row = 0; row < n; row++) {

    uint8_t spriteByte = pchip8->memory[pchip8->addressRegister + row];

    for(uint column = 0; column < 8; column++) {

      uint8_t spritePixel = spriteByte & ( 0x80u >> column );

      if(spritePixel > 0) {

        if(pchip8->screenPixels[((xPos + column) % LOGICAL_WIDTH) + LOGICAL_WIDTH * ((yPos + row) % LOGICAL_HEIGHT)] > 0) {
          pchip8->generalRegisters[0xF] = 1;
        }
        
        pchip8->screenPixels[((xPos + column) % LOGICAL_WIDTH) + LOGICAL_WIDTH * ((yPos + row) % LOGICAL_HEIGHT)] ^= 0xFFFFFFFF;
      }

    }
  }

}

static void opcode8XYN(Chip8 *restrict pchip8, uint16_t opcode) {

  uint8_t Vx = (opcode & 0x0F00) >> 0x8;
  uint8_t Vy = (opcode & 0x00F0) >> 0x4;
  uint8_t type = opcode & 0x000F;

  uint8_t prevValue;

  uint8_t *reg = pchip8->generalRegisters;

  switch(type) {
    case 0x0:
      reg[Vx] = reg[Vy];
      break;
    case 0x1:
      reg[Vx] |= reg[Vy];
      break;
    case 0x2:
      reg[Vx] &= reg[Vy];
      break;
    case 0x3:
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
      reg[0xF] = reg[Vx] < prevValue;
      break;
    case 0x6:
      prevValue = reg[Vx] & 0x1;
      reg[Vx] >>= 1;
      reg[0xF] = prevValue;
      break;
    case 0x7:
      prevValue = reg[Vy];
      reg[Vx] = reg[Vy] - reg[Vx];
      reg[0xF] = reg[Vx] <= prevValue;
      break;
    case 0xE:
      prevValue = (reg[Vx] & 0x80) >> 0x7;
      reg[Vx] <<= 1;
      reg[0xF] = prevValue;
      break;
    default:
      exit(EXIT_FAILURE);
      break;
  }

}

static void opcodeFXNN(Chip8 *restrict pchip8, uint16_t opcode){

  uint16_t address;

  uint8_t Vx = (opcode & 0x0F00) >> 0x8;
  uint8_t type = opcode & 0x00FF;

  switch(type) {
    case 0x07:
      pchip8->generalRegisters[Vx] = pchip8->delayTimer;
      break;
    case 0x0A:
      opcodeFX0A(pchip8, opcode);
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
      for(uint i = 0; i <= Vx; i++) {
        pchip8->memory[pchip8->addressRegister + i] = pchip8->generalRegisters[i];
      }
      break;
    case 0x65:
      for(uint i = 0; i <= Vx; i++) {
        pchip8->generalRegisters[i] = pchip8->memory[pchip8->addressRegister + i];
      }
      break;
    default:
      exit(EXIT_FAILURE);
      break;
  }

}

static void opcodeFX0A(Chip8 *restrict pchip8, uint16_t opcode) {

  uint8_t Vx = (opcode & 0x0F00) >> 8;

  if(pchip8->keyIsPressed) {
    if(*(pchip8->keypad[pchip8->pressedKey])) {
      pchip8->programCounter -= 2;
    } else {
      pchip8->keyIsPressed = false;
      pchip8->generalRegisters[Vx] = pchip8->pressedKey;
    }
  } else {
    for(uint i = 0; i <= 0xf; i++) {
      if(*(pchip8->keypad[i])) {
        pchip8->keyIsPressed = true;
        pchip8->pressedKey = i;
        break;
      }
    }
    pchip8->programCounter -= 2;
  }

}

static bool executeNextInstruction(AppContext *restrict pcontext, Chip8 *restrict pchip8) {

  uint16_t opcode = 0x0;
  uint16_t prevCode = 0x0;
  opcode |= pchip8->memory[pchip8->programCounter] << 0x8;
  pchip8->programCounter++;
  opcode |= pchip8->memory[pchip8->programCounter];
  pchip8->programCounter++;

  // if( prevCode != opcode ) SDL_Log("%04x", opcode);
  // uint16_t precode = opcode;

  uint8_t firstNybble = (opcode & 0xF000) >> 0xC;

  switch(firstNybble) {
    case 0x0:

      switch(opcode) {
        case 0xE0:
          memset(pchip8->screenPixels, 0, sizeof((pchip8->screenPixels)));
          break;
        case 0xEE:
          if( pchip8->stackPointer != 0) {
            pchip8->stackPointer--;
            pchip8->programCounter = pchip8->stack[pchip8->stackPointer];
          }
          break;
        default:
          break;
      }

      break;
    case 0x1:
      pchip8->programCounter = opcode & 0x0FFF;
      break;
    case 0x2:
      if( pchip8->stackPointer < 16) {
        pchip8->stack[pchip8->stackPointer] = pchip8->programCounter;
        pchip8->stackPointer++;
        pchip8->programCounter = opcode & 0x0FFF;
      } 
      break;
    case 0x3:
      if( pchip8->generalRegisters[(opcode & 0x0F00) >> 0x8] == (uint8_t)(opcode & 0x00FF) ) pchip8->programCounter += 2;
      break;
    case 0x4:
      if( pchip8->generalRegisters[(opcode & 0x0F00) >> 0x8] != (uint8_t)(opcode & 0x00FF) ) pchip8->programCounter += 2;
      break;
    case 0x5:
      if( pchip8->generalRegisters[(opcode & 0x0F00) >> 0x8] == pchip8->generalRegisters[(opcode & 0x00F0) >> 0x4] ) pchip8->programCounter += 2;
      break;
    case 0x6:
      pchip8->generalRegisters[(opcode & 0x0F00) >> 0x8] = opcode & 0x00FF;
      break;
    case 0x7:
      pchip8->generalRegisters[(opcode & 0x0F00) >> 0x8] += opcode & 0x00FF;
      break;
    case 0x8:
      opcode8XYN(pchip8, opcode);
      break;
    case 0x9:
      if( pchip8->generalRegisters[(opcode & 0x0F00) >> 0x8] != pchip8->generalRegisters[(opcode & 0x00F0) >> 0x4] ) pchip8->programCounter += 2;
      break;
    case 0xA:
      pchip8->addressRegister = opcode & 0x0FFF;
      break;
    case 0xB:
      pchip8->programCounter = (opcode & 0x0FFF) + pchip8->generalRegisters[0];
      break;
    case 0xC:
      pchip8->generalRegisters[ (opcode & 0x0F00) >> 0x8 ] = (rand() % 256) & (opcode & 0x00FF);
      break;
    case 0xD:
      opcodeDXYN(pchip8, opcode);
      break;
    case 0xE:

      switch(opcode & 0x00FF) {
        case 0x9E:
          if(*(pchip8->keypad[ pchip8->generalRegisters[(opcode & 0x0F00) >> 0x8] & 0xF])) pchip8->programCounter += 2;
          break;
        case 0xA1:
          if(!(*(pchip8->keypad[ pchip8->generalRegisters[(opcode & 0x0F00) >> 0x8] & 0xF]))) pchip8->programCounter += 2;
          break;
        default:
          break;
      }

      break;
    case 0xF:
      opcodeFXNN(pchip8, opcode);
      break;
    default:
      exit(EXIT_FAILURE);
      break;
  }

  return true;

}

static void quitApp(AppContext *restrict pcontext, Chip8 *restrict pchip8) {

  SDL_DestroyTexture(pcontext->ptexture);
  pcontext->ptexture = NULL;
  SDL_DestroyRenderer(pcontext->prenderer);
  pcontext->prenderer = NULL;
  SDL_DestroyWindow(pcontext->pwindow);
  pcontext->pwindow = NULL;

}


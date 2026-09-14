#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
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
    // This allows for specifying how fast we want the Chip8 ROM to run
    #define INSTRUCTION_FREQ_S 700.0f

    #define MS_PER_TIMER_DECREMENT (1000.0 / TIMER_FREQ_S)
    #define MS_PER_INSTRUCTION_EXECUTE (1000.0 / INSTRUCTION_FREQ_S)

// Defining the square wave that will play when the sound timer is non-zero
#define WAVE_AMPLITUDE 0.01f  
#define WAVE_CYCLE_PER_S 110.0f
#define SAMPLE_PER_S 44100.0f
#define SOUND_LENGTH_S 0.5f

#define GET_BIT(byte, bit) (byte & ( 0x80u >> bit ))
#define GET_VX(opcode) ((opcode & 0x0F00) >> 0x8)
#define GET_VY(opcode) ((opcode & 0x00F0) >> 0x4)
#define GET_N(opcode) (opcode & 0x000F)
#define GET_NN(opcode) (opcode & 0x00FF)
#define GET_NNN(opcode) (opcode & 0x0FFF)

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

  bool isClassic;

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

typedef struct AppState {
  AppContext *pcontext;
  Chip8 *pchip8;
  const char *restrict romFile;
} AppState;

typedef bool (*FamilyFunction)(AppState *restrict pstate, uint16_t opcode);

static bool startApp(AppState *restrict pstate);
static void quitApp(AppState *restrict pstate);

static bool initContext(AppContext *restrict pcontext);
static bool initChip8(Chip8 *restrict pchip8, const char *restrict romToLoad);

static void handleSDLEvents(AppContext *restrict pcontext);
static bool renderScreen(AppState *restrict pstate);
static bool loadROM(Chip8 *restrict pchip8, const char *restrict pfilePath);

static bool exeIntrucFamily0(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamily1(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamily2(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamily3(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamily4(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamily5(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamily6(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamily7(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamily8(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamily9(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamilyA(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamilyB(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamilyC(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamilyD(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamilyE(AppState *restrict pstate, uint16_t opcode);
static bool exeIntrucFamilyF(AppState *restrict pstate, uint16_t opcode);

static bool executeNextInstruction(AppState *restrict pstate);

FamilyFunction familyFunctions[0x10] = {
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
  exeIntrucFamilyF
};

int main(int argc, char *argv[]) {

  if(argc != 3) {
    SDL_LogError(SDL_SCANCODE_APPLICATION, "Usage: %s { -c | -m } <rom-file-path>", argv[0]);
    return EXIT_FAILURE;
  }

  bool isClassic;

  if( strcmp(argv[1], "-c") == 0) {
    isClassic = true;
  } else if ( strcmp(argv[1], "-m") == 0) {
    isClassic = false;
  } else {
    SDL_LogError(SDL_SCANCODE_APPLICATION, "Usage: %s { -c | -m } <rom-file-path>", argv[0]);
    return EXIT_FAILURE;
  }

  double instructionCounter = 0.0;
  double timerCounter = 0.0;
  double renderCounter = 0.0;

  int numSamples = (int)(SOUND_LENGTH_S * SAMPLE_PER_S);
  double numCycles = SOUND_LENGTH_S * WAVE_CYCLE_PER_S;

  double numSamplesPerCycle = numSamples / numCycles;

  double secondsPerCycle = 1.0f / WAVE_CYCLE_PER_S;

  float buff[numSamples];

  Uint64 nowTime, prevTime = 0;

  AppContext context = { 0 };
  Chip8 chip8 = { .isClassic = isClassic };
  AppState state = { &context, &chip8, argv[2]};

  if( !startApp(&state) ) {
    quitApp(&state);
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
    instructionCounter += (double)(nowTime - prevTime);
    timerCounter += (double)(nowTime - prevTime);
    renderCounter += (double)(nowTime - prevTime);
    prevTime = nowTime;

    while( instructionCounter >= MS_PER_INSTRUCTION_EXECUTE) {
      executeNextInstruction(&state);
      instructionCounter -= MS_PER_INSTRUCTION_EXECUTE;
    }

    while( timerCounter >= MS_PER_TIMER_DECREMENT ) {
      if(chip8.soundTimer > 0) chip8.soundTimer--;
      if(chip8.delayTimer > 0) chip8.delayTimer--;
      timerCounter -= MS_PER_TIMER_DECREMENT;
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

  quitApp(&state);

  return EXIT_SUCCESS;
}

static bool startApp(AppState *restrict pstate) {

  if( !SDL_SetAppMetadata("Chip 8 Emulator", "1.0", NULL) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set meta data: %s", SDL_GetError());
    return false;
  }

  if( !SDL_InitSubSystem(SDL_INIT_VIDEO |  SDL_INIT_AUDIO ) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to init video sub system: %s", SDL_GetError());
    return false;
  }

  if( !initContext(pstate->pcontext) ) {
    return false;
  }

  if( !initChip8(pstate->pchip8, pstate->romFile) ) {
    return false;
  }

  return true;

}

static void quitApp(AppState *restrict pstate) {

  AppContext *restrict pcontext = pstate->pcontext;

  SDL_DestroyTexture(pcontext->ptexture);
  pcontext->ptexture = NULL;
  SDL_DestroyRenderer(pcontext->prenderer);
  pcontext->prenderer = NULL;
  SDL_DestroyWindow(pcontext->pwindow);
  pcontext->pwindow = NULL;
  SDL_DestroyAudioStream(pcontext->stream);
  pcontext->stream = NULL;

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

static bool initChip8(Chip8 *restrict pchip8, const char *restrict romToLoad) {

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

  if( !loadROM(pchip8, romToLoad) ) return false;

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

static bool renderScreen(AppState *restrict pstate) {

  AppContext *restrict pcontext = pstate->pcontext;
  Chip8 *restrict pchip8 = pstate->pchip8;

  if( !SDL_UpdateTexture(pcontext->ptexture, NULL, pchip8->screenPixels, sizeof(pchip8->screenPixels[0]) * LOGICAL_WIDTH) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to update texture: %s", SDL_GetError());
    return false;
  }

  if( !SDL_RenderClear(pcontext->prenderer) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to render clear: %s", SDL_GetError());
    return false;
  }

  if( !SDL_RenderTexture(pcontext->prenderer, pcontext->ptexture, NULL, NULL) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to render texture: %s", SDL_GetError());
    return false;
  }

  if( !SDL_RenderPresent(pcontext->prenderer) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to render present: %s", SDL_GetError());
    return false;
  }

  return true;

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
    SDL_Log("Succesful full read from file: %s\nRead count: %lu", pfilePath, numRead);
  } else {
    SDL_Log("Succesful partial read from file: %s\nRead count: %lu", pfilePath, numRead);
  }

  fclose(pROM);

  return true;

}
static bool executeNextInstruction(AppState *restrict pstate) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  uint16_t opcode = 0x0;
  opcode |= pchip8->memory[pchip8->programCounter] << 0x8;
  pchip8->programCounter++;
  opcode |= pchip8->memory[pchip8->programCounter];
  pchip8->programCounter++;

  const uint8_t family = (opcode & 0xF000) >> 0xC;

  return familyFunctions[family](pstate, opcode);

}

static bool exeIntrucFamily0(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  switch(opcode) {
    case 0xE0:
      memset(pchip8->screenPixels, 0, sizeof((pchip8->screenPixels)));
      renderScreen(pstate);
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
static bool exeIntrucFamily1(AppState *restrict pstate, uint16_t opcode) {

  const uint16_t NNN = GET_NNN(opcode);

  pstate->pchip8->programCounter = NNN;

  return true;

}
static bool exeIntrucFamily2(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

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
static bool exeIntrucFamily3(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  if( pchip8->generalRegisters[Vx] == NN ) pchip8->programCounter += 2;

  return true;

}
static bool exeIntrucFamily4(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  if( pchip8->generalRegisters[Vx] != NN ) pchip8->programCounter += 2;

  return true;

}
static bool exeIntrucFamily5(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t Vy = GET_VY(opcode);

  if( pchip8->generalRegisters[Vx] == pchip8->generalRegisters[Vy] ) pchip8->programCounter += 2;
  
  return true;

}
static bool exeIntrucFamily6(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  pchip8->generalRegisters[Vx] = NN;

  return true;

}
static bool exeIntrucFamily7(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  pchip8->generalRegisters[Vx] += NN;

  return true;

}
static bool exeIntrucFamily8(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

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
      reg[0xF] = reg[Vx] < prevValue;
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
static bool exeIntrucFamily9(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t Vy = GET_VY(opcode);

  if( pchip8->generalRegisters[Vx] != pchip8->generalRegisters[Vy] ) pchip8->programCounter += 2;

  return true;

}
static bool exeIntrucFamilyA(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  const uint16_t NNN = GET_NNN(opcode);

  pchip8->addressRegister = NNN;

  return true;

}
static bool exeIntrucFamilyB(AppState *restrict pstate, uint16_t opcode) {

  // Is an ambiguous instruction, but according to Tvil it is most likely fine to leave it like this:
  //   https://tobiasvl.github.io/blog/write-a-chip-8-emulator/#bnnn-jump-with-offset

  Chip8 *restrict pchip8 = pstate->pchip8;

  const uint16_t NNN = GET_NNN(opcode);

  pchip8->programCounter = NNN + pchip8->generalRegisters[0];

  return true;

}
static bool exeIntrucFamilyC(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  pchip8->generalRegisters[Vx] = (rand() % 256) & NN;

  return true;

}
static bool exeIntrucFamilyD(AppState *restrict pstate, uint16_t opcode) {

  // When drawing sprites, the initial drawing location should wrap around the screen
  // While drawing however, the sprites should not wrap
  // https://tobiasvl.github.io/blog/write-a-chip-8-emulator/#dxyn-display

  Chip8 *restrict pchip8 = pstate->pchip8;

  uint32_t *restrict pixels = pchip8->screenPixels;
  uint8_t *restrict reg = pchip8->generalRegisters;

  const uint8_t *restrict mem = pchip8->memory;

  const uint16_t address = pchip8->addressRegister;

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t Vy = GET_VY(opcode);
  const uint8_t N = GET_N(opcode);

  const uint8_t xMax = LOGICAL_WIDTH;
  const uint8_t yMax = LOGICAL_HEIGHT;

  // Initial location is wrapped around the screen using modulo if it goes over either direction
  const uint8_t xPos = reg[Vx] % xMax;
  const uint8_t yPos = reg[Vy] % yMax;

  reg[0xF] = 0x0;

  for(uint row = 0; row < N; row++) {

    const uint8_t spriteByte = mem[address + row];

    for(uint column = 0; column < 8; column++) {

      // While drawing, the sprite DOES NOT wrap, this checks for that
      if((xPos + column) < xMax && (yPos + row) < yMax){

        const uint8_t spritePixel = GET_BIT(spriteByte, column);

        if(spritePixel > 0) {

          const uint16_t pixelIndex = (xPos + column) + LOGICAL_WIDTH * (yPos + row);

          if(pixels[pixelIndex] > 0) {
            reg[0xF] = 1;
          }

          pixels[pixelIndex] ^= 0xFFFFFFFF;

        }

      }      

    }
  }

  renderScreen(pstate);

  return true;
}
static bool exeIntrucFamilyE(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  const uint8_t Vx = GET_VX(opcode);

  switch(opcode & 0x00FF) {
    case 0x9E:
      if(*(pchip8->keypad[ pchip8->generalRegisters[Vx] & 0xF])) pchip8->programCounter += 2;
      break;
    case 0xA1:
      if(!(*(pchip8->keypad[ pchip8->generalRegisters[Vx] & 0xF]))) pchip8->programCounter += 2;
      break;
    default:
      return false;
      break;
  }

  return true;
}
static bool exeIntrucFamilyF(AppState *restrict pstate, uint16_t opcode) {

  Chip8 *restrict pchip8 = pstate->pchip8;

  uint16_t address;

  const uint8_t Vx = GET_VX(opcode);
  const uint8_t NN = GET_NN(opcode);

  switch(NN) {
    case 0x07:
      pchip8->generalRegisters[Vx] = pchip8->delayTimer;
      break;
    case 0x0A:
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
      if(pchip8->isClassic) pchip8->addressRegister += Vx + 1;
      break;
    case 0x65:
      for(uint i = 0; i <= Vx; i++) {
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
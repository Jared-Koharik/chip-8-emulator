#include "chip8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// Defining the dimensions of the Chip8 screen
#define LOGICAL_WIDTH 64
#define LOGICAL_HEIGHT 32

// Defining how many times per second certain actions are taken
  // Timers, Rendering, and Instruction execution
    // We want the timer to decrement 60 times a second per the Chip8 specification
    #define TIMER_FREQ_S 60.0f
    // This allows for specifying how fast we want the Chip8 ROM to run
    #define INSTRUCTION_FREQ_S 1000.0f

    #define MS_PER_TIMER_DECREMENT (1000.0 / TIMER_FREQ_S)
    #define MS_PER_INSTRUCTION_EXECUTE (1000.0 / INSTRUCTION_FREQ_S)

// Defining the square wave that will play when the sound timer is non-zero
#define WAVE_AMPLITUDE 0.1f
#define WAVE_CYCLE_PER_S 110
#define SAMPLE_PER_S 44100
#define MAX_SOUND_LENGTH_MS 500

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
} AppState;

typedef bool (*FamilyFunction)(AppState *restrict pstate, uint16_t opcode);

static bool startApp(AppState *restrict pstate, bool isClassic, const char *restrict romFile);
static void quitApp(AppState *restrict pstate);

static bool initContext(AppContext *restrict pcontext);

static void handleSDLEvents(AppContext *restrict pcontext);
static bool renderScreen(AppState *restrict pstate);

#define NUM_SAMPLES_PER_CYCLE ((SAMPLE_PER_S / WAVE_CYCLE_PER_S) % 2 == 1 ? (SAMPLE_PER_S / WAVE_CYCLE_PER_S + 1) : (SAMPLE_PER_S / WAVE_CYCLE_PER_S))
#define MAX_SAMPLES ((MAX_SOUND_LENGTH_MS * SAMPLE_PER_S) / 1000)

static float buff[MAX_SAMPLES] = {};
static float cycle[NUM_SAMPLES_PER_CYCLE] = {};
static int cycleIndex = 0;

int main(int argc, char *argv[]) {

  if(argc > 3) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: %s <rom-file-path> [ -c ]", argv[0]);
    return EXIT_FAILURE;
  }

  Chip8 chip8 = { 0 };
  AppContext context = { 0 };
  AppState state = { &context, &chip8 };

  bool isClassic = ( ( argc == 3 ) && ( strcmp(argv[2], "-c") == 0 ) );

  if( !startApp(&state, isClassic, argv[1]) ) {
    quitApp(&state);
    return EXIT_FAILURE;
  }
 
  Uint64 nowTime, prevTime = 0;

  double instructionCounter = 0.0;
  double timerCounter = 0.0;

  for (int sample = 0; sample < NUM_SAMPLES_PER_CYCLE; sample++) {
    cycle[sample] = sample < (NUM_SAMPLES_PER_CYCLE / 2) ? WAVE_AMPLITUDE : -WAVE_AMPLITUDE;
  }

  prevTime = SDL_GetTicks();

  while(!context.quit) {

    handleSDLEvents(&context);

    nowTime = SDL_GetTicks();
    instructionCounter += (double)(nowTime - prevTime);
    timerCounter += (double)(nowTime - prevTime);
    prevTime = nowTime;

    while( instructionCounter >= MS_PER_INSTRUCTION_EXECUTE) {
      executeNextInstruction(&chip8);
      instructionCounter -= MS_PER_INSTRUCTION_EXECUTE;
    }

    while( timerCounter >= MS_PER_TIMER_DECREMENT ) {
      if(chip8.soundTimer > 0) chip8.soundTimer--;
      if(chip8.delayTimer > 0) chip8.delayTimer--;
      timerCounter -= MS_PER_TIMER_DECREMENT;
    }

    if(chip8.soundTimer > 0) {
      if(SDL_GetAudioStreamQueued(context.stream) == 0) {

        int i;

        for(i = 0; i < MAX_SAMPLES; i++) {
          buff[i] = cycle[cycleIndex];
          cycleIndex = (cycleIndex + 1) % NUM_SAMPLES_PER_CYCLE;
        }

        SDL_PutAudioStreamData(context.stream, buff, i * sizeof(float));
      }
    } else {
      SDL_ClearAudioStream(context.stream);
    }

    if(chip8.readyToRender) renderScreen(&state);

    SDL_Delay(1);
  }

  quitApp(&state);

  return EXIT_SUCCESS;
}

static bool startApp(AppState *restrict pstate, bool isClassic, const char *restrict romFile) {

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

  if( !initChip8(pstate->pchip8, isClassic, romFile) ) {
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

static void handleSDLEvents(AppContext *restrict pcontext) {

  SDL_Event event;

  while(SDL_PollEvent(&event)) {

    switch(event.type) {

      case SDL_EVENT_QUIT:
        pcontext->quit = true;
        break;

      case SDL_EVENT_KEY_UP:

        switch(event.key.scancode) {
          case SDL_SCANCODE_ESCAPE:
            pcontext->quit = true;
            break;
          default:
            break;
        }

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
#include "chip8.h"
#include "audio.h"
#include "video.h"
#include "input.h"

#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define TIMER_FREQ_S 60.0f
#define INSTRUCTION_FREQ_S 1000.0f
#define MS_PER_TIMER_DECREMENT (1000.0 / TIMER_FREQ_S)
#define MS_PER_INSTRUCTION_EXECUTE (1000.0 / INSTRUCTION_FREQ_S)

typedef struct AppContext {
  VideoContext *pvcontext;
  AudioContext *pacontext;
  InputContext *picontext;
  Chip8 *pchip8;
  bool quit;
} AppContext;

static bool startApp(AppContext *restrict pcontext, const char *restrict option, const char *restrict romFile);
static bool initAppContext(AppContext *restrict pcontext, const char *restrict option, const char *restrict romFile);

static void handleSDLEvents(AppContext *restrict pcontext);

static void quitApp(AppContext *restrict pstate);
static void freeAppContext(AppContext *restrict pcontext);

int main(int argc, char *argv[]) {

  if(argc != 3) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: %s { -m | -c } <rom-file-path>", argv[0]);
    return EXIT_FAILURE;
  }

  VideoContext vcontext = { 0 };
  AudioContext acontext = { 0 };
  InputContext icontext = { 0 };
  Chip8 chip8 = { 0 };
  AppContext context = { &vcontext, &acontext, &icontext, &chip8, false };

  if( !startApp(&context, argv[1], argv[2]) ) {
    quitApp(&context);
    return EXIT_FAILURE;
  }
 
  Uint64 nowTime, prevTime = 0;

  double instructionCounter = 0.0;
  double timerCounter = 0.0;

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
      tickTimers(&chip8);
      timerCounter -= MS_PER_TIMER_DECREMENT;
    }

    if(checkSoundTimer(&chip8)) {
      playAudio(&acontext);
    } else {
      clearAudio(&acontext);
    }

    if(checkDrawFlag(&chip8)) { 
      renderPixelsToScreen(&vcontext, getPixels(&chip8));
    }

    SDL_Delay(1);
  }

  quitApp(&context);

  return EXIT_SUCCESS;
}

static bool startApp(AppContext *restrict pcontext, const char *restrict option, const char *restrict romFile) {

  if( !SDL_SetAppMetadata("Chip 8 Emulator", "1.0", NULL) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set meta data: %s", SDL_GetError());
    return false;
  }

  return initAppContext(pcontext, option, romFile);

}

static bool initAppContext(AppContext *restrict pcontext, const char *restrict option, const char *restrict romFile) {

  if( !initVideoContext(pcontext->pvcontext, PIXEL_WIDTH, PIXEL_HEIGHT, 10) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init video context");
    return false;
  }

  if( !initAudioContext(pcontext->pacontext) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init audio context");
    return false;
  }

  if( !initInputContext(pcontext->picontext) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init input context");
    return false;
  }

  if( !initChip8(pcontext->pchip8, strcmp(option, "-c") == 0, romFile) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init chip8");
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

      case SDL_EVENT_KEY_DOWN:
        handleKeyEvent(pcontext->pchip8, event.key.scancode, true);
        break;

      case SDL_EVENT_KEY_UP:
        handleKeyEvent(pcontext->pchip8, event.key.scancode, false);
        if(event.key.scancode == SDL_SCANCODE_ESCAPE) pcontext->quit = true;
        break;

      default:
        break;

    }

  }
}

static void quitApp(AppContext *restrict pcontext) {

  freeAppContext(pcontext);

}

static void freeAppContext(AppContext *restrict pcontext) {

  freeVideoContext(pcontext->pvcontext);
  freeAudioContext(pcontext->pacontext);
  freeInputContext(pcontext->picontext);

}
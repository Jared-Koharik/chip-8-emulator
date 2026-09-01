#include <stdlib.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define LOGICAL_WIDTH 64
#define LOGICAL_HEIGHT 32

typedef struct AppContext {
  SDL_Window* window;
  SDL_Renderer* renderer;
  int width, height;
} AppContext;

static bool setup(AppContext *restrict context);
static void close(AppContext *restrict context);

int main(void) {

  int quit = false;

  AppContext context = { 
    .window = NULL,
    .renderer = NULL,
    .width = LOGICAL_WIDTH * 10,
    .height = LOGICAL_HEIGHT * 10,
   };

  if( !setup(&context) ) {
    close(&context);
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

    SDL_RenderPresent(context.renderer);

  }

  close(&context);

  return EXIT_SUCCESS;
}

static bool setup(AppContext *restrict context) {

  if( !SDL_SetAppMetadata("Chip 8 Emulator", "1.0", NULL) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set meta data: %s", SDL_GetError());
    return false;
  }

  if( !SDL_InitSubSystem(SDL_INIT_VIDEO) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to init video sub system: %s", SDL_GetError());
    return false;
  }

  if( !SDL_CreateWindowAndRenderer("Chip 8 Emulator", context->width, context->height, 0x0, &(context->window), &(context->renderer)) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to create window: %s", SDL_GetError());
    return false;
  }

  if( !SDL_SetRenderLogicalPresentation(context->renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set logical representation to letterbox: %s", SDL_GetError());
    return false;
  }

  return true;

}

static void close(AppContext *restrict context) {

  SDL_DestroyRenderer(context->renderer);
  SDL_DestroyWindow(context->window);

}
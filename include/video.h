#ifndef VIDEO_H
#define VIDEO_H

#include <stdbool.h>
#include <SDL3/SDL.h>

typedef struct {
  SDL_Window *pwindow;
  SDL_Renderer *prenderer;
  SDL_Texture *ptexture;
  int logicalWidth;
  int logicalHeight;
  int scale;
} VideoContext;

bool initVideoContext(VideoContext *restrict pvcontext, int logicalWidth, int logicalHeight, int scale);
bool freeVideoContext(VideoContext *restrict pvcontext);

bool renderPixelsToScreen(VideoContext *restrict pstate, uint32_t *restrict pixels);

#endif

#include "video.h"

bool initVideoContext(VideoContext *restrict pvcontext, int logicalWidth, int logicalHeight, int scale) {

    if( !SDL_InitSubSystem( SDL_INIT_VIDEO ) ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to init video sub system: %s", SDL_GetError());
        return false;
    }

    if( !SDL_CreateWindowAndRenderer("Chip 8 Emulator", logicalWidth * scale, logicalHeight * scale, 0x0, &(pvcontext->pwindow), &(pvcontext->prenderer)) ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to create window: %s", SDL_GetError());
        return false;
    }

    if( !SDL_SetRenderDrawColor(pvcontext->prenderer, 0, 0, 0, 255) ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to set render draw color: %s", SDL_GetError());
        return false;
    }

    pvcontext->ptexture = SDL_CreateTexture(pvcontext->prenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, logicalWidth, logicalHeight);
    if( pvcontext->ptexture == NULL ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s", SDL_GetError());
        return false;
    }

    SDL_SetTextureScaleMode(pvcontext->ptexture, SDL_SCALEMODE_NEAREST);
    if( !SDL_SetRenderLogicalPresentation(pvcontext->prenderer, logicalWidth, logicalHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX) ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set render logical representation: %s", SDL_GetError());
        return false;
    }

    pvcontext->logicalWidth = logicalWidth;
    pvcontext->logicalHeight = logicalHeight;
    pvcontext->scale = scale;

    return true;

}

bool freeVideoContext(VideoContext *restrict pvcontext) {

  SDL_DestroyTexture(pvcontext->ptexture);
  pvcontext->ptexture = NULL;
  SDL_DestroyRenderer(pvcontext->prenderer);
  pvcontext->prenderer = NULL;
  SDL_DestroyWindow(pvcontext->pwindow);
  pvcontext->pwindow = NULL;

  return true;

}

bool renderPixelsToScreen(VideoContext *restrict pvcontext, uint32_t *restrict pixels) {

  if( !SDL_UpdateTexture(pvcontext->ptexture, NULL, pixels, pvcontext->logicalWidth * sizeof(pixels[0])) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to update texture: %s", SDL_GetError());
    return false;
  }

  if( !SDL_RenderClear(pvcontext->prenderer) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to render clear: %s", SDL_GetError());
    return false;
  }

  if( !SDL_RenderTexture(pvcontext->prenderer, pvcontext->ptexture, NULL, NULL) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to render texture: %s", SDL_GetError());
    return false;
  }

  if( !SDL_RenderPresent(pvcontext->prenderer) ) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to render present: %s", SDL_GetError());
    return false;
  }

  return true;

}
#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <SDL3/SDL.h>

typedef struct Chip8 Chip8;

typedef struct {
    bool keys[0x10];
} InputContext;

bool initInputContext(InputContext *restrict picontext);

void handleKeyEvent(Chip8 *restrict pchip8, SDL_Scancode scancode, bool isDown);

void freeInputContext(InputContext * restrict picontext);

#endif
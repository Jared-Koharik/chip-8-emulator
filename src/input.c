#include "input.h"
#include "chip8.h"

static const SDL_Scancode keypadKeys[KEYPAD_SIZE] = {
    SDL_SCANCODE_X, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
    SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_A,
    SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_Z, SDL_SCANCODE_C,
    SDL_SCANCODE_4, SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_V,
};

bool initInputContext(InputContext *restrict picontext) {
    return true;
}

void handleKeyEvent(Chip8 *restrict pchip8, SDL_Scancode scancode, bool isDown) {

    for(uint8_t i = 0; i < KEYPAD_SIZE; i++) {
        if(keypadKeys[i] == scancode) {
            pchip8->keypad[i] = isDown;
            return;
        }
    }

}

void freeInputContext(InputContext * restrict picontext) {

}
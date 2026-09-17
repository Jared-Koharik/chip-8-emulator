#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <SDL3/SDL.h>

typedef struct {
  SDL_AudioStream *stream;
  SDL_AudioSpec inputSpec;
  SDL_AudioDeviceID deviceID;
} AudioContext;

bool initAudioContext(AudioContext *restrict pacontext);

bool playAudio(AudioContext *restrict pacontext);
bool clearAudio(AudioContext *restrict pacontext);

bool freeAudioContext(AudioContext *restrict pacontext);

#endif
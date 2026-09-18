#include "audio.h"

#define WAVE_AMPLITUDE 0.1f
#define CYCLE_PER_S 100
#define SAMPLE_PER_S 50000
#define MAX_SOUND_LENGTH_MS 50

#define NUM_SAMPLES_PER_CYCLE ((SAMPLE_PER_S / CYCLE_PER_S) % 2 == 1 ? (SAMPLE_PER_S / CYCLE_PER_S + 1) : (SAMPLE_PER_S / CYCLE_PER_S))
#define MAX_SAMPLES ((MAX_SOUND_LENGTH_MS * SAMPLE_PER_S) / 1000)

static float buff[MAX_SAMPLES] = { 0 };
static float cycle[NUM_SAMPLES_PER_CYCLE] = { 0 };
static int cycleIndex = 0;

bool initAudioContext(AudioContext *pacontext) {

    if( !SDL_InitSubSystem( SDL_INIT_AUDIO ) ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failure to init audio sub system: %s", SDL_GetError());
        return false;
    }

    pacontext->deviceID = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if( pacontext->deviceID == 0 ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio device: %s", SDL_GetError());
        return false;
    }

    pacontext->inputSpec.format = SDL_AUDIO_F32;
    pacontext->inputSpec.channels = 1;
    pacontext->inputSpec.freq = SAMPLE_PER_S;

    pacontext->stream = SDL_CreateAudioStream(&(pacontext->inputSpec), NULL);
    if( pacontext->stream == NULL ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create audio stream: %s", SDL_GetError());
        return false;
    }

    if( !SDL_BindAudioStream(pacontext->deviceID, pacontext->stream) ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to bind audio stream to device: %s", SDL_GetError());
        return false;
    }

    if( !SDL_ResumeAudioStreamDevice(pacontext->stream) ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to resume audio stream device: %s", SDL_GetError());
        return false;
    }

    for (uint32_t sample = 0; sample < NUM_SAMPLES_PER_CYCLE; sample++) {
        cycle[sample] = sample < (NUM_SAMPLES_PER_CYCLE / 2) ? WAVE_AMPLITUDE : 0.0f;
    }
    
    return true;

}

bool playAudio(AudioContext *restrict pacontext) {

    int numQueued = SDL_GetAudioStreamQueued(pacontext->stream);
    if( numQueued == -1 ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get audio stream queued: %s", SDL_GetError());
        return false;
    }

    if(numQueued == 0) {
        for(uint32_t i = 0; i < MAX_SAMPLES; i++) {
            buff[i] = cycle[cycleIndex];
            cycleIndex = (cycleIndex + 1) % NUM_SAMPLES_PER_CYCLE;
        }

        if( !SDL_PutAudioStreamData(pacontext->stream, buff, MAX_SAMPLES * sizeof(float)) ) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to put audio stream data: %s", SDL_GetError());
            return false;
        }
    }

    return true;

}

bool clearAudio(AudioContext *restrict pacontext) {

    float tempBuff;
    if( SDL_GetAudioStreamData(pacontext->stream, &tempBuff, sizeof(float)) == -1 ) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get audio stream data: %s", SDL_GetError());
        return false;
    }

    if( tempBuff == 0.0f ) {
        if( !SDL_ClearAudioStream(pacontext->stream) ) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to clear audio stream: %s", SDL_GetError());
            return false;
        }
        cycleIndex = 0;
    }

    return true;
}

bool freeAudioContext(AudioContext *restrict pacontext) {

  SDL_DestroyAudioStream(pacontext->stream);
  pacontext->stream = NULL;

  return true;

}




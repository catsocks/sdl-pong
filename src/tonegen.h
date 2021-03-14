#pragma once

#include <SDL.h>
#include <stdbool.h>

#include "math.h"

#define TONEGEN_SAMPLES_PER_SECOND 44100
#define TONEGEN_FORMAT_SIZE sizeof(int16_t) // sample format
#define TONEGEN_BUFFER_MAX_LENGTH (TONEGEN_SAMPLES_PER_SECOND / 10)

extern const SDL_AudioSpec TONEGEN_AUDIO_SPEC;

struct tonegen {
    int amplitude;
    int freq;
    uint32_t sample_idx;
    int remaining_samples; // samples yet to be generated
    int16_t buffer[TONEGEN_BUFFER_MAX_LENGTH];
    size_t buffer_size;
    bool mute;
};

struct tonegen make_tonegen(float volume_percentage);
void set_tonegen_tone(struct tonegen *gen, int freq, int duration_ms);
void tonegen_generate(struct tonegen *gen, SDL_AudioDeviceID device_id);
void tonegen_queue(struct tonegen *gen, SDL_AudioDeviceID device_id);

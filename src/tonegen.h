#pragma once

#include <SDL.h>

#include "math.h"

#define TONEGEN_SAMPLES_PER_SECOND 44100
#define TONEGEN_FORMAT_SIZE sizeof(int16_t) // sample format
#define TONEGEN_BUFFER_MAX_LENGTH (TONEGEN_SAMPLES_PER_SECOND / 10)

struct tonegen_tone {
    int freq;
    int duration; // in ms
};

struct tonegen {
    SDL_AudioDeviceID device_id;
    int amplitude;
    int freq;
    uint32_t sample_idx;
    int remaining_samples; // samples yet to be generated
    int16_t buffer[TONEGEN_BUFFER_MAX_LENGTH];
    size_t buffer_size;
};

struct tonegen make_tonegen(SDL_AudioDeviceID device_id,
                            float volume_percentage);
void set_tonegen_tone(struct tonegen *gen, struct tonegen_tone tone);
void tonegen_generate(struct tonegen *gen);
void tonegen_queue(struct tonegen *gen);

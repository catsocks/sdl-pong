#pragma once

#include <SDL.h>

#include "math.h"

#define TONEGEN_SAMPLING_RATE 44100         // samples per second
#define TONEGEN_FORMAT_SIZE sizeof(int16_t) // sample format
#define TONEGEN_BUFFER_MAX_LEN (TONEGEN_SAMPLING_RATE / 10)
#define TONEGEN_BUFFER_MAX_SIZE (TONEGEN_BUFFER_MAX_LEN * TONEGEN_FORMAT_SIZE)

struct tonegen_tone {
    int freq;
    int duration; // in ms
};

struct tonegen {
    int freq;
    int sample_idx;
    int remaining_samples; // to be generated
    int16_t buffer[TONEGEN_BUFFER_MAX_SIZE];
    size_t buffer_size;
};

void tonegen_set_tone(struct tonegen *gen, struct tonegen_tone tone);
void tonegen_generate(struct tonegen *gen);
void tonegen_queue(struct tonegen *gen, SDL_AudioDeviceID device_id);

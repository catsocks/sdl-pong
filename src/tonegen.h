#pragma once

#include <SDL.h>

#include "math.h"

#define TONEGEN_SAMPLING_RATE 44100
#define TONEGEN_FORMAT_SIZE sizeof(int16_t) // audio sample format size
#define TONEGEN_BUF_MAX_LENGTH (TONEGEN_SAMPLING_RATE / 10)
#define TONEGEN_BUF_MAX_SIZE (TONEGEN_BUF_MAX_LENGTH * TONEGEN_FORMAT_SIZE)

struct tone {
    int freq;
    int duration; // in ms
};

struct tonegen {
    int freq;
    int sample_idx;
    int remaining_samples; // samples to be generated
    int16_t buf[TONEGEN_BUF_MAX_SIZE];
    size_t buf_size;
};

void tonegen_set_tone(struct tonegen *gen, struct tone tone);
void tonegen_generate(struct tonegen *gen);
void tonegen_queue(struct tonegen *gen, SDL_AudioDeviceID device_id);

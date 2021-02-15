#pragma once

#include <SDL.h>

#include "math.h"

#define TONEGEN_SAMPLING_RATE 44100         // samples per second
#define TONEGEN_FORMAT_SIZE sizeof(int16_t) // sample format
#define TONEGEN_BUFFER_MAX_LEN (TONEGEN_SAMPLING_RATE / 10)

struct tonegen_tone {
    int freq;
    int duration; // in ms
};

struct tonegen {
    int amplitude;
    int freq;
    int sample_idx;
    int remaining_samples; // samples yet to be generated
    int16_t buffer[TONEGEN_BUFFER_MAX_LEN];
    size_t buffer_size;
};

void tonegen_set_tone(struct tonegen *gen, struct tonegen_tone tone);
struct tonegen make_tonegen(float volume_percentage);
void tonegen_generate(struct tonegen *gen);
void tonegen_queue(struct tonegen *gen, SDL_AudioDeviceID device_id);

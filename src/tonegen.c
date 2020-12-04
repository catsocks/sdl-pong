#include "tonegen.h"

static const int FORMAT_MAX_VALUE = INT16_MAX; // determ. by TONEGEN_FORMAT_SIZE
static const int AMPLITUDE = 0.025 * FORMAT_MAX_VALUE; // volume

void tonegen_set_tone(struct tonegen *gen, struct tonegen_tone tone) {
    gen->freq = tone.freq;
    gen->sample_idx = 0;
    gen->remaining_samples = (tone.duration / 1000.0) * TONEGEN_SAMPLING_RATE;
}

static int square_wave_sample(int idx, int freq) {
    if (sin(2.0 * M_PI * freq * (idx / (double)TONEGEN_SAMPLING_RATE)) >= 0.0) {
        return AMPLITUDE;
    }
    return -AMPLITUDE;
}

void tonegen_generate(struct tonegen *gen) {
    int len = gen->remaining_samples;
    if (len > TONEGEN_BUFFER_MAX_LEN) {
        len = TONEGEN_BUFFER_MAX_LEN;
    }
    gen->remaining_samples -= len;
    gen->buffer_size = len * TONEGEN_FORMAT_SIZE;

    for (int i = 0; i < len; i++) {
        gen->buffer[i] = square_wave_sample(gen->sample_idx + i, gen->freq);
    }
    gen->sample_idx += len;
}

void tonegen_queue(struct tonegen *gen, SDL_AudioDeviceID device_id) {
    if (gen->buffer_size > 0) {
        if (SDL_QueueAudio(device_id, gen->buffer, gen->buffer_size) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Couldn't queue audio: %s", SDL_GetError());
        }
    }
}

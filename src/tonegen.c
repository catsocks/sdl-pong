#include "tonegen.h"

const int FORMAT_MAX_VALUE = INT16_MAX;
const float AMPLITUDE = 0.025 * FORMAT_MAX_VALUE; // volume, 2.5%

void tonegen_set_tone(struct tonegen *gen, struct tone tone) {
    gen->freq = tone.freq;
    gen->sample_idx = 0;
    gen->remaining_samples = (tone.duration / 1000.0) * TONEGEN_SAMPLING_RATE;
}

static int square_wave_sample(int idx, int freq) {
    if (sin(2 * M_PI * freq * (idx / (double)TONEGEN_SAMPLING_RATE)) >= 0) {
        return AMPLITUDE;
    }
    return -AMPLITUDE;
}

void tonegen_generate(struct tonegen *gen) {
    if (gen->remaining_samples == 0) {
        if (gen->buf_size > 0) {
            gen->buf_size = 0;
        }
        return;
    }

    int len = gen->remaining_samples;
    if (len > TONEGEN_BUF_MAX_LENGTH) {
        len = TONEGEN_BUF_MAX_LENGTH;
    }
    gen->remaining_samples -= len;
    gen->buf_size = len * TONEGEN_FORMAT_SIZE;

    for (int i = 0; i < len; i++) {
        gen->buf[i] = square_wave_sample(gen->sample_idx + i, gen->freq);
    }
    gen->sample_idx += len;
}

void tonegen_queue(struct tonegen *gen, SDL_AudioDeviceID device_id) {
    if (gen->buf_size > 0) {
        if (SDL_QueueAudio(device_id, gen->buf, gen->buf_size) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Couldn't queue audio: %s", SDL_GetError());
        }
    }
}

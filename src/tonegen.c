#include "tonegen.h"

static const int FORMAT_MAX_VALUE = INT16_MAX; // determ. by TONEGEN_FORMAT_SIZE

struct tonegen make_tonegen(float volume_percentage) {
    return (struct tonegen){
        .amplitude = (volume_percentage / 100.0f) * FORMAT_MAX_VALUE,
    };
}

void set_tonegen_tone(struct tonegen *gen, struct tonegen_tone tone) {
    gen->freq = tone.freq;
    gen->remaining_samples =
        (tone.duration / 1000.0) * TONEGEN_SAMPLES_PER_SECOND;
}

static int square_wave_sample(int sample_idx, int freq, int amplitude) {
    int wave_period = TONEGEN_SAMPLES_PER_SECOND / freq;
    int half_period = wave_period / 2;
    if ((sample_idx / half_period) % 2 == 0) {
        return amplitude;
    }
    return -amplitude;
}

void tonegen_generate(struct tonegen *gen) {
    int len = gen->remaining_samples;
    if (len > TONEGEN_BUFFER_MAX_LENGTH) {
        len = TONEGEN_BUFFER_MAX_LENGTH;
    }
    gen->remaining_samples -= len;
    gen->buffer_size = len * TONEGEN_FORMAT_SIZE;

    for (int i = 0; i < len; i++) {
        gen->buffer[i] =
            square_wave_sample(gen->sample_idx + i, gen->freq, gen->amplitude);
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

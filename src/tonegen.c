#include "tonegen.h"

static const int FORMAT_MAX_VALUE = INT16_MAX; // determ. by TONEGEN_FORMAT_SIZE

const SDL_AudioSpec TONEGEN_AUDIO_SPEC = {
    .freq = TONEGEN_SAMPLES_PER_SECOND,
    .format = AUDIO_S16SYS,
    .channels = 1,
    .samples = 4096,
};

struct tonegen make_tonegen(float volume_percentage) {
    return (struct tonegen){
        .amplitude = (volume_percentage / 100.0f) * FORMAT_MAX_VALUE,
    };
}

void set_tonegen_tone(struct tonegen *gen, int freq, int duration_ms) {
    gen->freq = freq;
    gen->remaining_samples =
        (duration_ms / 1000.0) * TONEGEN_SAMPLES_PER_SECOND;
}

static int square_wave_sample(int sample_idx, int freq, int amplitude) {
    int wave_period = TONEGEN_SAMPLES_PER_SECOND / freq;
    int half_period = wave_period / 2;
    if ((sample_idx / half_period) % 2 == 0) {
        return amplitude;
    }
    return -amplitude;
}

void tonegen_generate(struct tonegen *gen, SDL_AudioDeviceID device_id) {
    size_t queue_size = SDL_GetQueuedAudioSize(device_id);
    int max_len =
        TONEGEN_BUFFER_MAX_LENGTH - (queue_size / TONEGEN_FORMAT_SIZE);
    if (max_len < 0) {
        max_len = 0;
    }
    int len = gen->remaining_samples;
    if (len > max_len) {
        len = max_len;
    }
    gen->remaining_samples -= len;
    gen->buffer_size = len * TONEGEN_FORMAT_SIZE;

    int amplitude = gen->amplitude;
    if (gen->mute) {
        amplitude = 0;
    }

    for (int i = 0; i < len; i++) {
        gen->buffer[i] =
            square_wave_sample(gen->sample_idx + i, gen->freq, amplitude);
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

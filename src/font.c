#include "font.h"

#define DIGIT_POINTS_MAX_LEN 10
#define DIGITS_LENGTH 10

const float DIGIT_WIDTH = 0.8;
const float DIGIT_SPREAD_FACTOR = 0.3;

const float NUMBER_DIGIT_GAP_FACTOR = 1;

struct digit {
    SDL_FPoint points[DIGIT_POINTS_MAX_LEN];
    int points_len;
};

struct digit digits[DIGITS_LENGTH] = {
    // 0
    {{{-0.4, 1},
      {0.4, 1},
      {0.4, 1},
      {0.4, -1},
      {0.4, -1},
      {-0.4, -1},
      {-0.4, 1},
      {-0.4, -1}},
     8},
    // 1
    {{{0.4, 1}, {0.4, -1}}, 2},
    // 2
    {{{-0.4, 1},
      {0.4, 1},
      {0.4, 1},
      {0.4, 0.1},
      {0.4, 0.1},
      {-0.4, 0.1},
      {-0.4, 0.1},
      {-0.4, -1},
      {-0.4, -1},
      {0.4, -1}},
     10},
    // 3
    {{{0.4, 1},
      {0.4, -1},
      {-0.4, 1},
      {0.4, 1},
      {-0.4, 0.1},
      {0.4, 0.1},
      {-0.4, -1},
      {0.4, -1}},
     8},
    // 4
    {{{0.4, 1},
      {0.4, -1},
      {-0.4, 1},
      {-0.4, 0.1},
      {0.4, 1},
      {0.4, 0.1},
      {0.4, 0.1},
      {-0.4, 0.1}},
     8},
    // 5
    {{{-0.4, 1},
      {0.4, 1},
      {-0.4, 1},
      {-0.4, 0.1},
      {-0.4, 0.1},
      {0.4, 0.1},
      {0.4, 0.1},
      {0.4, -1},
      {-0.4, -1},
      {0.4, -1}},
     10},
    // 6
    {{{-0.4, 1},
      {-0.4, -1},
      {-0.4, 0.1},
      {0.4, 0.1},
      {0.4, 0.1},
      {0.4, -1},
      {0.4, -1},
      {-0.4, -1}},
     8},
    // 7
    {{{0.4, 1}, {0.4, -1}, {-0.4, 1}, {0.4, 1}}, 4},
    // 8
    {{{0.4, 1},
      {0.4, -1},
      {-0.4, 1},
      {-0.4, -1},
      {-0.4, 1},
      {0.4, 1},
      {-0.4, -1},
      {0.4, -1},
      {-0.4, 0.1},
      {0.4, 0.1}},
     10},
    // 9
    {{{0.4, 1},
      {0.4, -1},
      {-0.4, 0.1},
      {0.4, 0.1},
      {-0.4, 0.1},
      {-0.4, 1},
      {-0.4, 1},
      {0.4, 1}},
     8},
};

static float rendered_digit_width(int height) {
    return height * ((DIGIT_WIDTH / 2) + (DIGIT_SPREAD_FACTOR / 2));
}

static float rendered_number_digit_gap(int width) {
    return width * NUMBER_DIGIT_GAP_FACTOR;
}

// The top-left corner of the digit will be equal to pos.
static void render_digit(SDL_Renderer *renderer, SDL_FPoint pos, int height,
                         int digit_num) {
    float scale_factor = height / 2.0;
    float spread = scale_factor * DIGIT_SPREAD_FACTOR;

    struct digit digit = digits[digit_num % DIGITS_LENGTH];
    for (int i = 0; i < digit.points_len; i += 2) {
        float x1 = digit.points[i].x;
        float y1 = digit.points[i].y;

        float x2 = digit.points[(i + 1) % digit.points_len].x;
        float y2 = digit.points[(i + 1) % digit.points_len].y;

        x1 *= scale_factor;
        x2 *= scale_factor;
        y1 *= -scale_factor + (spread / 2);
        y2 *= -scale_factor + (spread / 2);

        x1 += pos.x + scale_factor * (DIGIT_WIDTH / 2);
        x2 += pos.x + scale_factor * (DIGIT_WIDTH / 2);
        y1 += pos.y + scale_factor - (spread / 2);
        y2 += pos.y + scale_factor - (spread / 2);

        SDL_RenderFillRectF(renderer, &(SDL_FRect){.x = x1,
                                                   .y = y1,
                                                   .w = spread + (x2 - x1),
                                                   .h = spread + (y2 - y1)});
    }
}

// The top-right corner of the rendered number will be equal to pos.
void render_number(SDL_Renderer *renderer, SDL_FPoint pos, int height,
                   int num) {
    float width = rendered_digit_width(height);
    float gap = rendered_number_digit_gap(width);
    do {
        int digit = num % 10;
        pos.x -= width;
        render_digit(renderer, pos, height, digit);
        pos.x -= gap;
    } while ((num /= 10) != 0);
}

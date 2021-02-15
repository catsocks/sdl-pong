#include "digits.h"

#define POINTS_LIST_MAX_LENGTH 10

#define DIGITS_LENGTH 10

static const float DIGIT_HALF_WIDTH = 0.4f; // determ. by the DIGITS points
static const float DIGIT_LINE_SPREAD_FACTOR = 0.3f;

struct points {
    SDL_FPoint list[POINTS_LIST_MAX_LENGTH];
    int list_length;
};

static const struct points DIGITS[DIGITS_LENGTH] = {
    // 0
    {{{-0.4f, 1.0f},
      {0.4f, 1.0f},
      {0.4f, 1.0f},
      {0.4f, -1.0f},
      {0.4f, -1.0f},
      {-0.4f, -1.0f},
      {-0.4f, 1.0f},
      {-0.4f, -1.0f}},
     8},
    // 1
    {{{0.4f, 1.0f}, {0.4f, -1.0f}}, 2},
    // 2
    {{{-0.4f, 1.0f},
      {0.4f, 1.0f},
      {0.4f, 1.0f},
      {0.4f, 0.1f},
      {0.4f, 0.1f},
      {-0.4f, 0.1f},
      {-0.4f, 0.1f},
      {-0.4f, -1.0f},
      {-0.4f, -1.0f},
      {0.4f, -1.0f}},
     10},
    // 3
    {{{0.4f, 1.0f},
      {0.4f, -1.0f},
      {-0.4f, 1.0f},
      {0.4f, 1.0f},
      {-0.4f, 0.1f},
      {0.4f, 0.1f},
      {-0.4f, -1.0f},
      {0.4f, -1.0f}},
     8},
    // 4
    {{{0.4f, 1.0f},
      {0.4f, -1.0f},
      {-0.4f, 1.0f},
      {-0.4f, 0.1f},
      {0.4f, 1.0f},
      {0.4f, 0.1f},
      {0.4f, 0.1f},
      {-0.4f, 0.1f}},
     8},
    // 5
    {{{-0.4f, 1.0f},
      {0.4f, 1.0f},
      {-0.4f, 1.0f},
      {-0.4f, 0.1f},
      {-0.4f, 0.1f},
      {0.4f, 0.1f},
      {0.4f, 0.1f},
      {0.4f, -1.0f},
      {-0.4f, -1.0f},
      {0.4f, -1.0f}},
     10},
    // 6
    {{{-0.4f, 1.0f},
      {-0.4f, -1.0f},
      {-0.4f, 0.1f},
      {0.4f, 0.1f},
      {0.4f, 0.1f},
      {0.4f, -1.0f},
      {0.4f, -1.0f},
      {-0.4f, -1.0f}},
     8},
    // 7
    {{{0.4f, 1.0f}, {0.4f, -1.0f}, {-0.4f, 1.0f}, {0.4f, 1.0f}}, 4},
    // 8
    {{{0.4f, 1.0f},
      {0.4f, -1.0f},
      {-0.4f, 1.0f},
      {-0.4f, -1.0f},
      {-0.4f, 1.0f},
      {0.4f, 1.0f},
      {-0.4f, -1.0f},
      {0.4f, -1.0f},
      {-0.4f, 0.1f},
      {0.4f, 0.1f}},
     10},
    // 9
    {{{0.4f, 1.0f},
      {0.4f, -1.0f},
      {-0.4f, 0.1f},
      {0.4f, 0.1f},
      {-0.4f, 0.1f},
      {-0.4f, 1.0f},
      {-0.4f, 1.0f},
      {0.4f, 1.0f}},
     8},
};

static float rendered_digit_width(int height) {
    return height * ((DIGIT_LINE_SPREAD_FACTOR / 2.0f) + DIGIT_HALF_WIDTH);
}

// The top-left corner of the digit will be equal to given position.
static void render_digit(SDL_Renderer *renderer, SDL_FPoint position,
                         int height, int digit) {
    float scale_factor = height / 2.0f;
    float line_spread = scale_factor * DIGIT_LINE_SPREAD_FACTOR;

    struct points points = DIGITS[digit % DIGITS_LENGTH];
    for (int i = 0; i < points.list_length; i += 2) {
        SDL_FPoint p1 = points.list[i];
        SDL_FPoint p2 = points.list[(i + 1) % points.list_length];

        p1.x *= scale_factor;
        p2.x *= scale_factor;
        p1.x += position.x + (scale_factor * DIGIT_HALF_WIDTH);
        p2.x += position.x + (scale_factor * DIGIT_HALF_WIDTH);

        p1.y *= -scale_factor + (line_spread / 2.0f);
        p2.y *= -scale_factor + (line_spread / 2.0f);
        p1.y += position.y + scale_factor - (line_spread / 2.0f);
        p2.y += position.y + scale_factor - (line_spread / 2.0f);

        SDL_RenderFillRectF(renderer,
                            &(SDL_FRect){.x = p1.x,
                                         .y = p1.y,
                                         .w = line_spread + (p2.x - p1.x),
                                         .h = line_spread + (p2.y - p1.y)});
    }
}

// The top-right corner of the rendered digits will be equal to given position.
void render_digits(SDL_Renderer *renderer, SDL_FPoint position, int height,
                   int number) {
    float width = rendered_digit_width(height);
    do {
        int digit = number % 10;
        position.x -= width;
        render_digit(renderer, position, height, digit);
        position.x -= width; // gap
    } while ((number /= 10) != 0);
}

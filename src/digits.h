#pragma once

#include <SDL.h>

#include "renderer.h"

void render_digits(struct renderer_wrapper renderer, SDL_FPoint position,
                   int height, int number);

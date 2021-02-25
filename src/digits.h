#pragma once

#include <SDL.h>

#include "renderer.h"

void render_digits(struct renderer_wrapper renderer_wrapper,
                   SDL_FPoint position, int height, int number);

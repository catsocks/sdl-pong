#pragma once

#include <SDL.h>

// NOTE: Ditch this when SDL_RenderSetLogicalSize works correctly in the SDL
// Emscripten port when the game is made fullscreen.
struct renderer_wrapper {
    SDL_Renderer *renderer;
    SDL_Rect output_size;
    SDL_Rect logical_size;
    SDL_Rect viewport;
    float scale;
};

struct renderer_wrapper make_renderer_wrapper(SDL_Renderer *renderer,
                                              int logical_width,
                                              int logical_height);
void update_renderer_wrapper(struct renderer_wrapper *wrapper);
SDL_FRect scale_frect(struct renderer_wrapper wrapper, SDL_FRect rect);

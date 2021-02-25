#include "renderer.h"

struct renderer_wrapper make_renderer_wrapper(SDL_Renderer *renderer,
                                              int logical_width,
                                              int logical_height) {
    return (struct renderer_wrapper){
        .renderer = renderer,
        .logical_size =
            {
                .w = logical_width,
                .h = logical_height,
            },
    };
}

void update_renderer_wrapper(struct renderer_wrapper *wrapper) {
    SDL_GetRendererOutputSize(wrapper->renderer, &wrapper->output_size.w,
                              &wrapper->output_size.h);

    wrapper->scale =
        fminf(wrapper->output_size.h / (float)wrapper->logical_size.h,
              wrapper->output_size.w / (float)wrapper->logical_size.w);

    wrapper->viewport.x =
        (wrapper->output_size.w - (wrapper->scale * wrapper->logical_size.w)) /
        2.0;
    wrapper->viewport.y =
        (wrapper->output_size.h - (wrapper->scale * wrapper->logical_size.h)) /
        2.0;
}

SDL_FRect scale_frect(struct renderer_wrapper wrapper, SDL_FRect rect) {
    rect.x *= wrapper.scale;
    rect.y *= wrapper.scale;
    rect.w *= wrapper.scale;
    rect.h *= wrapper.scale;
    rect.x += wrapper.viewport.x;
    rect.y += wrapper.viewport.y;
    return rect;
}

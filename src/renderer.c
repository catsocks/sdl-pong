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

int renderer_wrapper_event_watch(void *userdata, SDL_Event *event) {
    struct renderer_wrapper *wrapper = (struct renderer_wrapper *)userdata;

    // The code below has been adapted from SDL_RendererEventWatch.
    if (event->type == SDL_FINGERDOWN || event->type == SDL_FINGERUP ||
        event->type == SDL_FINGERMOTION) {
        if (wrapper->output_size.w == 0.0f) {
            event->tfinger.x = 0.5f;
        } else {
            float normalized_viewport_y =
                wrapper->viewport.x / (float)wrapper->output_size.w;
            float normalized_viewport_h =
                wrapper->viewport.w / (float)wrapper->output_size.w;
            if (event->tfinger.x <= normalized_viewport_y) {
                event->tfinger.x = 0.0f;
            } else if (event->tfinger.x >=
                       (normalized_viewport_y + normalized_viewport_h)) {
                event->tfinger.x = 1.0f;
            } else {
                event->tfinger.x = (event->tfinger.x - normalized_viewport_y) /
                                   normalized_viewport_h;
            }
        }

        if (wrapper->output_size.h == 0.0f) {
            event->tfinger.y = 0.5f;
        } else {
            float normalized_viewport_y =
                wrapper->viewport.y / (float)wrapper->output_size.h;
            float normalized_viewport_h =
                wrapper->viewport.h / (float)wrapper->output_size.h;
            if (event->tfinger.y <= normalized_viewport_y) {
                event->tfinger.y = 0.0f;
            } else if (event->tfinger.y >=
                       (normalized_viewport_y + normalized_viewport_h)) {
                event->tfinger.y = 1.0f;
            } else {
                event->tfinger.y = (event->tfinger.y - normalized_viewport_y) /
                                   normalized_viewport_h;
            }
        }
    }

    return 0;
}

void update_renderer_wrapper(struct renderer_wrapper *wrapper) {
    SDL_GetRendererOutputSize(wrapper->renderer, &wrapper->output_size.w,
                              &wrapper->output_size.h);

    wrapper->scale =
        fminf(wrapper->output_size.h / (float)wrapper->logical_size.h,
              wrapper->output_size.w / (float)wrapper->logical_size.w);

    wrapper->viewport.w = wrapper->scale * wrapper->logical_size.w;
    wrapper->viewport.h = wrapper->scale * wrapper->logical_size.h;
    wrapper->viewport.x =
        (wrapper->output_size.w - (wrapper->viewport.w)) / 2.0;
    wrapper->viewport.y =
        (wrapper->output_size.h - (wrapper->viewport.h)) / 2.0;
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

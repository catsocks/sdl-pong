#include <SDL.h>
#include <stdbool.h>
#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "game.h"
#include "math.h"
#include "renderer.h"
#include "tonegen.h"

#ifndef DEBUGGING
#define DEBUGGING false
#endif

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

struct context {
    SDL_Window *window;
    struct renderer_wrapper renderer_wrapper;
    bool quit_requested;
    struct game game;
    struct tonegen tonegen;
    uint64_t current_time;
    struct player_input player_1_input;
    struct player_input player_2_input;
    uint32_t last_center_finger_down_timestamp;
    SDL_FingerID last_center_finger_down_finger_id;
};

void main_loop(void *arg);
void check_controller_added_event(struct context *ctx, SDL_Event event);
void check_controller_removed_event(struct context *ctx, SDL_Event event);
void check_finger_down_event(struct context *ctx, SDL_Event event);
void check_finger_up_event(struct context *ctx, SDL_Event event);
void check_finger_motion_event(struct context *ctx, SDL_Event event);
void check_keydown_event(struct context *ctx, SDL_Event event);
void toggle_fullscreen(struct context *ctx);

int main(int argc, char *argv[]) {
    // Suppress unused variable warnings, SDL requires that main accept argc and
    // argv when using MSVC or MinGW.
    (void)argc;
    (void)argv;

    srand(time(NULL));

#if DEBUGGING
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
#endif

    uint32_t flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(flags) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_AudioDeviceID audio_device_id =
        SDL_OpenAudioDevice(NULL, 0, &TONEGEN_AUDIO_SPEC, NULL, 0);
    if (audio_device_id == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't open an audio device: %s", SDL_GetError());
    }

    // Unpause the audio device because it is paused by default.
    SDL_PauseAudioDevice(audio_device_id, 0);

    // Create a hidden window so it may only be shown after the game is mostly
    // initialized.
    SDL_Window *window = SDL_CreateWindow(
        "Tennis", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't create renderer: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    struct context ctx = {
        .window = window,
        .renderer_wrapper =
            make_renderer_wrapper(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT),
        .game = make_game(false),
        .tonegen = make_tonegen(audio_device_id, 2.5f),
        .current_time = SDL_GetPerformanceCounter(),
    };

    SDL_AddEventWatch(renderer_wrapper_event_watch, &ctx.renderer_wrapper);

    SDL_ShowWindow(window);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop, &ctx, 0, 1);
#else
    while (!ctx.quit_requested) {
        main_loop(&ctx);
    }
#endif

    SDL_GameControllerClose(ctx.player_1_input.controller);
    SDL_GameControllerClose(ctx.player_2_input.controller);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_CloseAudioDevice(audio_device_id);

    SDL_Quit();

    return EXIT_SUCCESS;
}

void main_loop(void *arg) {
    struct context *ctx = arg;

    struct game *game = &ctx->game;

    uint64_t previous_time = ctx->current_time;
    ctx->current_time = SDL_GetPerformanceCounter();
    double frame_time = (ctx->current_time - previous_time) /
                        (double)SDL_GetPerformanceFrequency();

    SDL_Event event = {0};
    while (SDL_PollEvent(&event) == 1) {
        switch (event.type) {
        case SDL_QUIT:
            ctx->quit_requested = true;
            break;
        case SDL_KEYDOWN:
            check_keydown_event(ctx, event);
            break;
        case SDL_FINGERDOWN:
            check_finger_down_event(ctx, event);
            break;
        case SDL_FINGERUP:
            check_finger_up_event(ctx, event);
            break;
        case SDL_FINGERMOTION:
            check_finger_motion_event(ctx, event);
            break;
        case SDL_CONTROLLERDEVICEADDED:
            check_controller_added_event(ctx, event);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            check_controller_removed_event(ctx, event);
            break;
        }
    }

    update_renderer_wrapper(&ctx->renderer_wrapper);

    check_input_inactivity(ctx->player_1_input, &game->ghost_1);
    check_input_inactivity(ctx->player_2_input, &game->ghost_2);

    set_ghost_velocity(&game->ghost_1, game->paddle_1, game->ghost_ball);
    set_ghost_velocity(&game->ghost_2, game->paddle_2, game->ghost_ball);

    check_paddle_controls(&game->paddle_1, &game->ghost_1,
                          &ctx->player_1_input);
    check_paddle_controls(&game->paddle_2, &game->ghost_2,
                          &ctx->player_2_input);

    while (!ctx->game.paused && frame_time > 0.0) {
        double max_frame_time = 1 / 60.0;
        double delta_time = fmin(frame_time, max_frame_time);

        update_paddle(&game->paddle_1, delta_time);
        update_paddle(&game->paddle_2, delta_time);
        update_ball(&game->ball, delta_time, game->time);
        update_ball(&game->ghost_ball, delta_time, game->time);

        check_ball_hit_wall(game);
        check_paddle_missed_ball(game);
        check_paddle_hit_ball(game);

        check_round_over(game);
        check_round_restart_timeout(game);

        frame_time -= delta_time;
        game->time += delta_time;
    }

    check_game_events(&ctx->game, &ctx->tonegen);

    SDL_SetRenderDrawColor(ctx->renderer_wrapper.renderer, 0, 0, 0,
                           255); // black
    SDL_RenderClear(ctx->renderer_wrapper.renderer);

    SDL_SetRenderDrawColor(ctx->renderer_wrapper.renderer, 255, 255, 255,
                           255); // white

    render_score(ctx->renderer_wrapper, game->paddle_1);
    render_score(ctx->renderer_wrapper, game->paddle_2);

    render_net(ctx->renderer_wrapper);
    render_paddle(ctx->renderer_wrapper, game, game->paddle_1);
    render_paddle(ctx->renderer_wrapper, game, game->paddle_2);
    render_ball(ctx->renderer_wrapper, game->ball);
    if (game->debug_mode) {
        debug_render_ghost_ball(ctx->renderer_wrapper, game->ghost_ball);
    }

    tonegen_generate(&ctx->tonegen);
    tonegen_queue(&ctx->tonegen);

    SDL_RenderPresent(ctx->renderer_wrapper.renderer);
}

void check_controller_added_event(struct context *ctx, SDL_Event event) {
    if (ctx->player_1_input.controller == NULL) {
        ctx->player_1_input.controller =
            SDL_GameControllerOpen(event.cdevice.which);
    } else if (ctx->player_2_input.controller == NULL) {
        ctx->player_2_input.controller =
            SDL_GameControllerOpen(event.cdevice.which);
    }
}

void check_controller_removed_event(struct context *ctx, SDL_Event event) {
    SDL_Joystick *joystick =
        SDL_GameControllerGetJoystick(ctx->player_1_input.controller);
    if (SDL_JoystickInstanceID(joystick) == event.cdevice.which) {
        SDL_GameControllerClose(ctx->player_1_input.controller);
        ctx->player_1_input.controller = NULL;
        return;
    }
    joystick = SDL_GameControllerGetJoystick(ctx->player_2_input.controller);
    if (SDL_JoystickInstanceID(joystick) == event.cdevice.which) {
        SDL_GameControllerClose(ctx->player_2_input.controller);
        ctx->player_2_input.controller = NULL;
    }
}

void toggle_fullscreen(struct context *ctx) {
    if (SDL_GetWindowFlags(ctx->window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
        SDL_SetWindowFullscreen(ctx->window, 0);
    } else {
        SDL_SetWindowFullscreen(ctx->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
}

void check_finger_down_event(struct context *ctx, SDL_Event event) {
    if (event.tfinger.x < 0.3f) {
        ctx->player_1_input.touch_id = event.tfinger.touchId;
        ctx->player_1_input.finger_id = event.tfinger.fingerId;
        ctx->player_1_input.finger_y = event.tfinger.y * WINDOW_HEIGHT;
        ctx->player_1_input.finger_down = true;
    } else if (event.tfinger.x > 0.7f) {
        ctx->player_2_input.touch_id = event.tfinger.touchId;
        ctx->player_2_input.finger_id = event.tfinger.fingerId;
        ctx->player_2_input.finger_y = event.tfinger.y * WINDOW_HEIGHT;
        ctx->player_2_input.finger_down = true;
    } else {
        unsigned time_since_last_finger_down =
            event.tfinger.timestamp - ctx->last_center_finger_down_timestamp;
        bool same_finger =
            ctx->last_center_finger_down_finger_id == event.tfinger.fingerId;
        unsigned max_delay = 500; // in ms
        if (time_since_last_finger_down < max_delay && same_finger) {
            toggle_fullscreen(ctx);
        }
        ctx->last_center_finger_down_timestamp = event.tfinger.timestamp;
        ctx->last_center_finger_down_finger_id = event.tfinger.fingerId;
    }
}

void check_finger_motion_event(struct context *ctx, SDL_Event event) {
    if (ctx->player_1_input.touch_id == event.tfinger.touchId) {
        if (ctx->player_1_input.finger_id == event.tfinger.fingerId) {
            ctx->player_1_input.finger_y = event.tfinger.y * WINDOW_HEIGHT;
        }
    }
    if (ctx->player_2_input.touch_id == event.tfinger.touchId) {
        if (ctx->player_2_input.finger_id == event.tfinger.fingerId) {
            ctx->player_2_input.finger_y = event.tfinger.y * WINDOW_HEIGHT;
        }
    }
}

void check_finger_up_event(struct context *ctx, SDL_Event event) {
    if (ctx->player_1_input.touch_id == event.tfinger.touchId) {
        if (ctx->player_1_input.finger_id == event.tfinger.fingerId) {
            ctx->player_1_input.touch_id = 0;
            ctx->player_1_input.finger_id = 0;
            ctx->player_1_input.finger_down = false;
        }
    }
    if (ctx->player_2_input.touch_id == event.tfinger.touchId) {
        if (ctx->player_2_input.finger_id == event.tfinger.fingerId) {
            ctx->player_2_input.touch_id = 0;
            ctx->player_2_input.finger_id = 0;
            ctx->player_2_input.finger_down = false;
        }
    }
}

void check_keydown_event(struct context *ctx, SDL_Event event) {
    switch (event.key.keysym.sym) {
    case SDLK_F11:
        toggle_fullscreen(ctx);
        break;
    case SDLK_m:
        ctx->tonegen.mute = !ctx->tonegen.mute;
        break;
    case SDLK_r:
        restart_round(&ctx->game);
        break;
    case SDLK_p:
        ctx->game.paused = !ctx->game.paused;
        break;
    case SDLK_1:
        if (ctx->game.cheats_enabled) {
            ctx->game.paddle_1.score += 1;
        }
        break;
    case SDLK_2:
        if (ctx->game.cheats_enabled) {
            ctx->game.paddle_2.score += 1;
        }
        break;
    case SDLK_d:
        if (event.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT)) {
            // Ctrl + Shift + D
            ctx->game.debug_mode = !ctx->game.debug_mode;
        }
        break;
    }
}

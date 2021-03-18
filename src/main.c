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
    struct game game;
    struct renderer_wrapper renderer;
    SDL_AudioDeviceID audio_device_id;
    bool quit_requested;
    uint64_t current_time;
};

void main_loop(void *arg);

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
        .game = make_game(window, DEBUGGING),
        .renderer =
            make_renderer_wrapper(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT),
        .audio_device_id = audio_device_id,
        .current_time = SDL_GetPerformanceCounter(),
    };

    SDL_AddEventWatch(renderer_wrapper_event_watch, &ctx.renderer);

    SDL_ShowWindow(window);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop, &ctx, 0, 1);
#else
    while (!ctx.quit_requested) {
        main_loop(&ctx);
    }
#endif

    SDL_GameControllerClose(ctx.game.player_1_input.controller);
    SDL_GameControllerClose(ctx.game.player_2_input.controller);

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
            check_keydown_event(game, event);
            break;
        case SDL_FINGERDOWN:
            check_finger_down_event(game, event);
            break;
        case SDL_FINGERUP:
            check_finger_up_event(game, event);
            break;
        case SDL_FINGERMOTION:
            check_finger_motion_event(game, event);
            break;
        case SDL_CONTROLLERDEVICEADDED:
            check_controller_added_event(game, event);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            check_controller_removed_event(game, event);
            break;
        }
    }

    update_renderer_wrapper(&ctx->renderer);

    check_player_activity(game, game->player_1_input, &game->ghost_1);
    check_player_activity(game, game->player_2_input, &game->ghost_2);

    set_ghost_velocity(&game->ghost_1, game->paddle_1, game->ghost_ball);
    set_ghost_velocity(&game->ghost_2, game->paddle_2, game->ghost_ball);

    check_paddle_controls(&game->paddle_1, &game->ghost_1,
                          &game->player_1_input);
    check_paddle_controls(&game->paddle_2, &game->ghost_2,
                          &game->player_2_input);

    while (!game->paused && frame_time > 0.0) {
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

    check_game_events(game);

    SDL_SetRenderDrawColor(ctx->renderer.renderer, 0, 0, 0,
                           255); // black
    SDL_RenderClear(ctx->renderer.renderer);

    SDL_SetRenderDrawColor(ctx->renderer.renderer, 255, 255, 255,
                           255); // white

    render_score(ctx->renderer, game->paddle_1);
    render_score(ctx->renderer, game->paddle_2);

    render_net(ctx->renderer);
    render_paddle(ctx->renderer, game, game->paddle_1);
    render_paddle(ctx->renderer, game, game->paddle_2);
    render_ball(ctx->renderer, game->ball);
    if (game->debug_mode) {
        debug_render_ghost_ball(ctx->renderer, game->ghost_ball);
    }

    tonegen_generate(&game->tonegen, ctx->audio_device_id);
    tonegen_queue(&game->tonegen, ctx->audio_device_id);

    SDL_RenderPresent(ctx->renderer.renderer);
}

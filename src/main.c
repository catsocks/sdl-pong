#include <SDL.h>
#include <stdbool.h>
#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "digits.h"
#include "math.h"
#include "tonegen.h"

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

const int NET_WIDTH = 5;
const int NET_HEIGHT = 15;

const struct tonegen_tone SCORE_TONE = {240, 510};
const struct tonegen_tone PADDLE_HIT_TONE = {480, 35};
const struct tonegen_tone WALL_HIT_TONE = {240, 20};

struct ghost {
    int idle_offset;
    float speed;
    float bias;
    bool inactive;
};

struct paddle {
    int no;
    SDL_FRect rect;
    float velocity; // vertical
    float max_speed;
    int score;
};

struct ball {
    SDL_FRect rect;
    SDL_FPoint velocity;
    bool served;
    uint32_t serve_timeout;
};

struct game {
    bool cheats_enabled;
    struct paddle paddle_1;
    struct paddle paddle_2;
    struct ghost ghost_1;
    struct ghost ghost_2;
    struct ball ball;
    int max_score;
    bool round_over;
    uint32_t round_restart_timeout;
    uint32_t last_player_1_input;
    uint32_t last_player_2_input;
};

struct context {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_GameController *controller_1;
    SDL_GameController *controller_2;
    bool quit_requested;
    struct game game;
    struct tonegen tonegen;
    uint64_t current_time;
    uint32_t last_finger_down_event;
};

void main_loop(void *arg);
struct game make_game();
void check_controller_added_event(struct context *ctx, SDL_Event event);
void check_controller_removed_event(struct context *ctx, SDL_Event event);
void check_finger_down_event(struct context *ctx, SDL_Event event);
void check_keydown_event(struct context *ctx, SDL_Event event);
void toggle_fullscreen(struct context *ctx);
struct paddle make_paddle(int no);
struct ghost make_ghost();
struct ball make_ball(int paddle_no, bool round_over);
bool check_paddle_controls(struct paddle *paddle, struct ghost *ghost,
                           SDL_GameController *ctrl);
void check_inactive_player(uint32_t last_player_input, struct ghost *ghost);
void ghost_control_paddle(struct ghost *ghost, struct paddle *paddle,
                          struct ball ball, double dt);
void update_paddle(struct paddle *paddle, double dt);
void randomize_ghost_speed(struct ghost *ghost);
void randomize_ghost_bias(struct ghost *ghost);
void randomize_ghost_idle_offset(struct ghost *ghost);
void update_ball(struct ball *ball, double dt);
void check_ball_hit_wall(struct game *game, struct tonegen *tonegen);
void check_paddle_hit_ball(struct game *game, struct tonegen *tonegen);
void check_paddle_missed_ball(struct game *game, struct tonegen *tonegen);
void check_round_over(struct game *game);
void restart_round(struct game *game);
void check_round_restart_timeout(struct game *game);
void render_score(SDL_Renderer *renderer, struct paddle paddle);
void render_net(SDL_Renderer *renderer);
void render_paddle(SDL_Renderer *renderer, struct game *game,
                   struct paddle paddle);
void render_ball(SDL_Renderer *renderer, struct ball ball);

int main(int argc, char *argv[]) {
    // Suppress unused variable warnings, SDL requires that main accept argc and
    // argv when using MSVC or MinGW.
    (void)argc;
    (void)argv;

    // For choosing which paddle gets served the ball first and the vertical
    // position and angle of the ball every time it is served.
    srand(time(NULL));

    uint32_t flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(flags) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    // Try to open an audio device for playing mono signed 16-bit samples.
    SDL_AudioDeviceID audio_device_id =
        SDL_OpenAudioDevice(NULL, 0,
                            &(SDL_AudioSpec){.freq = TONEGEN_SAMPLES_PER_SECOND,
                                             .format = AUDIO_S16SYS,
                                             .channels = 1,
                                             .samples = 2048},
                            NULL, 0);
    if (audio_device_id == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't open an audio device: %s", SDL_GetError());
    }

    // Unpause the audio device which is paused by default.
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

    // For automatic resolution-indenpendent scaling.
    SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

    SDL_ShowWindow(window);

    struct context ctx = {
        .window = window,
        .renderer = renderer,
        .current_time = SDL_GetPerformanceCounter(),
        .game = make_game(),
        .tonegen = make_tonegen(audio_device_id, 2.5f),
    };

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop, &ctx, 0, 1);
#else
    while (!ctx.quit_requested) {
        main_loop(&ctx);
    }
#endif

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
    double delta_time = (ctx->current_time - previous_time) /
                        (double)SDL_GetPerformanceFrequency();
    delta_time = fmin(delta_time, 1 / 60.0);

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
        case SDL_CONTROLLERDEVICEADDED:
            check_controller_added_event(ctx, event);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            check_controller_removed_event(ctx, event);
            break;
        }
    }

    if (check_paddle_controls(&game->paddle_1, &game->ghost_1,
                              ctx->controller_1)) {
        game->last_player_1_input = SDL_GetTicks();
    }
    if (check_paddle_controls(&game->paddle_2, &game->ghost_2,
                              ctx->controller_2)) {
        game->last_player_2_input = SDL_GetTicks();
    }

    check_inactive_player(game->last_player_1_input, &game->ghost_1);
    check_inactive_player(game->last_player_2_input, &game->ghost_2);

    ghost_control_paddle(&game->ghost_1, &game->paddle_1, game->ball,
                         delta_time);
    ghost_control_paddle(&game->ghost_2, &game->paddle_2, game->ball,
                         delta_time);

    update_paddle(&game->paddle_1, delta_time);
    update_paddle(&game->paddle_2, delta_time);
    update_ball(&game->ball, delta_time);

    check_ball_hit_wall(game, &ctx->tonegen);
    check_paddle_missed_ball(game, &ctx->tonegen);
    check_paddle_hit_ball(game, &ctx->tonegen);

    check_round_over(game);
    check_round_restart_timeout(game);

    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255); // black
    SDL_RenderClear(ctx->renderer);

    SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 255, 255); // white

    render_score(ctx->renderer, game->paddle_1);
    render_score(ctx->renderer, game->paddle_2);

    render_net(ctx->renderer);
    render_paddle(ctx->renderer, game, game->paddle_1);
    render_paddle(ctx->renderer, game, game->paddle_2);
    render_ball(ctx->renderer, game->ball);

    tonegen_generate(&ctx->tonegen);
    tonegen_queue(&ctx->tonegen);

    SDL_RenderPresent(ctx->renderer);
}

struct game make_game() {
    struct game game = {0};
    game.paddle_1 = make_paddle(1);
    game.paddle_2 = make_paddle(2);
    game.ghost_1 = make_ghost();
    game.ghost_2 = make_ghost();
    game.ball = make_ball(rand_range(1, 2), false);
    game.max_score = 11;
    return game;
}

void check_controller_added_event(struct context *ctx, SDL_Event event) {
    if (ctx->controller_1 == NULL) {
        ctx->controller_1 = SDL_GameControllerOpen(event.cdevice.which);
    } else if (ctx->controller_2 == NULL) {
        ctx->controller_2 = SDL_GameControllerOpen(event.cdevice.which);
    }
}

void check_controller_removed_event(struct context *ctx, SDL_Event event) {
    SDL_Joystick *joystick = SDL_GameControllerGetJoystick(ctx->controller_1);
    if (SDL_JoystickInstanceID(joystick) == event.cdevice.which) {
        SDL_GameControllerClose(ctx->controller_1);
        ctx->controller_1 = NULL;
        return;
    }
    joystick = SDL_GameControllerGetJoystick(ctx->controller_2);
    if (SDL_JoystickInstanceID(joystick) == event.cdevice.which) {
        SDL_GameControllerClose(ctx->controller_2);
        ctx->controller_2 = NULL;
    }
}

void toggle_fullscreen(struct context *ctx) {
#ifdef __EMSCRIPTEN__
    // SDL_SetWindowFullscreen has Emscripten support but it seems to break the
    // effects of SDL_RenderSetLogicalSize, making the canvas element be
    // displayed in fullscreen also appears to break it, so the body element is
    // made fullscreen instead.
    (void)ctx; // suppress unused variable warning

    EmscriptenFullscreenChangeEvent event = {0};
    emscripten_get_fullscreen_status(&event);
    if (event.isFullscreen) {
        emscripten_exit_fullscreen();
        return;
    }

    EmscriptenFullscreenStrategy strategy = {
        .scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_DEFAULT,
        .canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF,
        .filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT,
    };
    emscripten_request_fullscreen_strategy("#body", true, &strategy);
#else
    if (SDL_GetWindowFlags(ctx->window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
        SDL_SetWindowFullscreen(ctx->window, 0);
    } else {
        SDL_SetWindowFullscreen(ctx->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
#endif
}

void check_finger_down_event(struct context *ctx, SDL_Event event) {
    unsigned max_delay = 500; // in ms
    if (event.tfinger.timestamp - ctx->last_finger_down_event < max_delay) {
        toggle_fullscreen(ctx);
    }
    ctx->last_finger_down_event = event.tfinger.timestamp;
}

void check_keydown_event(struct context *ctx, SDL_Event event) {
    switch (event.key.keysym.sym) {
    case SDLK_F11:
        toggle_fullscreen(ctx);
        break;
    case SDLK_r:
        if (SDL_GetModState() & KMOD_SHIFT) {
            restart_round(&ctx->game);
        }
        break;
    case SDLK_m:
        ctx->tonegen.mute = !ctx->tonegen.mute;
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
    }
}

struct paddle make_paddle(int no) {
    struct paddle paddle = {0};
    paddle.no = no;
    paddle.rect.w = 10.0f;
    paddle.rect.h = 50.0f;
    float margin = 50.0f;
    paddle.rect.x = (paddle.no == 1) ? margin : WINDOW_WIDTH - margin;
    paddle.rect.y = (WINDOW_HEIGHT - paddle.rect.h) / 2.0f;
    paddle.max_speed = 500.0f;
    return paddle;
}

struct ghost make_ghost() {
    struct ghost ghost = {0};
    randomize_ghost_speed(&ghost);
    randomize_ghost_bias(&ghost);
    return ghost;
}

// Return a ball that is on the side of the net of the given paddle with its
// velocity set so it moves at a random angle towards the paddle.
struct ball make_ball(int paddle_no, bool round_over) {
    struct ball ball = {0};

    int size = 14;
    ball.rect.w = size;
    ball.rect.h = size;
    ball.rect.x = (WINDOW_WIDTH - ball.rect.w) / 2.0f;
    ball.rect.x += NET_WIDTH * ((paddle_no == 1) ? -2.0f : 2.0f);
    ball.rect.y = frand_range(0.0f, WINDOW_HEIGHT - ball.rect.h);

    float angle = frand_range(-1.0f, 1.0f) * (M_PI / 6.0f);
    if (paddle_no == 1) {
        angle += M_PI;
    }
    int speed = 360;
    ball.velocity.x = cosf(angle) * speed;
    ball.velocity.y = -sinf(angle) * speed;

    if (!round_over) {
        ball.serve_timeout = SDL_GetTicks() + 2000; // in ms
    }

    return ball;
}

bool check_paddle_controls(struct paddle *paddle, struct ghost *ghost,
                           SDL_GameController *ctrl) {
    int velocity = 0;
    if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_UP) ==
        SDL_PRESSED) {
        velocity = -paddle->max_speed;
    } else if (SDL_GameControllerGetButton(
                   ctrl, SDL_CONTROLLER_BUTTON_DPAD_DOWN) == SDL_PRESSED) {
        velocity = paddle->max_speed;
    }

    int up_key = SDL_SCANCODE_W;
    int down_key = SDL_SCANCODE_S;
    if (paddle->no == 2) {
        up_key = SDL_SCANCODE_UP;
        down_key = SDL_SCANCODE_DOWN;
    }
    const uint8_t *state = SDL_GetKeyboardState(NULL);
    if (state[up_key] == SDL_PRESSED) {
        velocity = -paddle->max_speed;
    } else if (state[down_key] == SDL_PRESSED) {
        velocity = paddle->max_speed;
    }

    if (velocity == 0) {
        if (ghost->inactive) {
            paddle->velocity = 0;
        }
        return false;
    }
    paddle->velocity = velocity;
    ghost->inactive = true;
    return true;
}

void check_inactive_player(uint32_t last_player_input, struct ghost *ghost) {
    int timeout = 5000; // in ms
    if (SDL_GetTicks() > last_player_input + timeout) {
        ghost->inactive = false;
    }
}

void ghost_control_paddle(struct ghost *ghost, struct paddle *paddle,
                          struct ball ball, double dt) {
    if (ghost->inactive) {
        return;
    }

    float target = ((WINDOW_HEIGHT - paddle->rect.h) / 2) + ghost->idle_offset;
    if (ball.served) {
        float bias = (paddle->rect.h / 2.0f) * ghost->bias;
        target = ball.rect.y - ((paddle->rect.h - ball.rect.h) / 2) + bias;
    }

    float ball_distance = fabsf(ball.rect.x - paddle->rect.x);
    float threshold = WINDOW_WIDTH / 1.1;
    ball_distance = fminf(ball_distance, threshold) / threshold;

    float dest_distance = fabsf(target - paddle->rect.y);
    threshold = paddle->rect.h / 2;
    dest_distance = fminf(dest_distance, threshold) / threshold;

    float speed = ghost->speed * (1.0f - ball_distance) * dest_distance;
    speed *= paddle->max_speed;

    paddle->rect.y = move_towards(paddle->rect.y, target, speed * dt);
}

void update_paddle(struct paddle *paddle, double dt) {
    paddle->rect.y += paddle->velocity * dt;
    paddle->rect.y =
        clamp(paddle->rect.y, 0.0f, WINDOW_HEIGHT - paddle->rect.h);
}

void randomize_ghost_speed(struct ghost *ghost) {
    ghost->speed = frand_range(0.85f, 0.90f);
}

void randomize_ghost_bias(struct ghost *ghost) {
    ghost->bias = frand_range(-1.0f, 1.0f);
}

void randomize_ghost_idle_offset(struct ghost *ghost) {
    int max_distance = WINDOW_HEIGHT / 8;
    ghost->idle_offset = rand_range(-max_distance, max_distance);
}

void update_ball(struct ball *ball, double dt) {
    if (ball->served) {
        ball->rect.x += ball->velocity.x * dt;
        ball->rect.y += ball->velocity.y * dt;
    } else if (SDL_GetTicks() >= ball->serve_timeout) {
        ball->served = true;
    }
}

void check_ball_hit_wall(struct game *game, struct tonegen *tonegen) {
    struct ball *ball = &game->ball;

    // The ball will always bounce off vertical walls.
    if (ball->rect.y < 0.0f || ball->rect.y + ball->rect.h > WINDOW_HEIGHT) {
        ball->velocity.y *= -1.0f;
        if (!game->round_over) {
            set_tonegen_tone(tonegen, WALL_HIT_TONE);
        }

        ball->rect.y = clamp(ball->rect.y, 0.0f, WINDOW_HEIGHT - ball->rect.h);
    }

    // The ball will only bounce off horizontal walls when the round is over.
    if (game->round_over) {
        if (ball->rect.x < 0.0f || ball->rect.x + ball->rect.h > WINDOW_WIDTH) {
            ball->velocity.x *= -1.0f;
            ball->rect.x =
                clamp(ball->rect.x, 0.0f, WINDOW_WIDTH - ball->rect.w);
        }
    }
}

// Return whether there is an intersection between the horizontal half of a
// paddle facing the net, and the ball.
bool paddle_intersects_ball(struct paddle paddle, struct ball ball) {
    bool y_intersect = paddle.rect.y < ball.rect.y + ball.rect.h &&
                       paddle.rect.y + paddle.rect.h > ball.rect.y;
    if (paddle.no == 1) {
        return paddle.rect.x + (paddle.rect.w / 2.0f) <
                   ball.rect.x + ball.rect.w &&
               paddle.rect.x + paddle.rect.w > ball.rect.x && y_intersect;
    }
    return paddle.rect.x < ball.rect.x + ball.rect.w &&
           paddle.rect.x + (paddle.rect.w / 2.0f) > ball.rect.x && y_intersect;
}

void bounce_ball_off_paddle(struct ball *ball, struct paddle *paddle) {
    // Relative to the center of the paddle and the ball.
    float intersect = paddle->rect.y + (paddle->rect.h / 2.0f) - ball->rect.y -
                      (ball->rect.h / 2.0f);

    float max_bounce_angle = M_PI / 4.0f;
    float bounce_angle =
        (intersect / (paddle->rect.h / 2.0)) * max_bounce_angle;

    // The length of the velocity vector.
    float speed = sqrtf((ball->velocity.y * ball->velocity.y) +
                        (ball->velocity.x * ball->velocity.x));

    // Increment speed if it hasn't reached the limit.
    if (speed < 550) {
        speed += 20;
    }

    if (paddle->no == 1) {
        ball->rect.x = paddle->rect.x + paddle->rect.w;
    } else {
        ball->rect.x = paddle->rect.x - ball->rect.w;
        bounce_angle = M_PI - bounce_angle; // flip angle horizontally
    }

    ball->velocity.x = cosf(bounce_angle) * speed;
    ball->velocity.y = -sinf(bounce_angle) * speed;
}

void check_paddle_hit_ball(struct game *game, struct tonegen *tonegen) {
    if (!game->round_over) {
        if (paddle_intersects_ball(game->paddle_1, game->ball)) {
            bounce_ball_off_paddle(&game->ball, &game->paddle_1);
            randomize_ghost_bias(&game->ghost_2);
        } else if (paddle_intersects_ball(game->paddle_2, game->ball)) {
            bounce_ball_off_paddle(&game->ball, &game->paddle_2);
            randomize_ghost_bias(&game->ghost_1);
        } else {
            return;
        }
        set_tonegen_tone(tonegen, PADDLE_HIT_TONE);
    }
}

void check_paddle_missed_ball(struct game *game, struct tonegen *tonegen) {
    if (game->ball.rect.x + game->ball.rect.w < 0) {
        // Paddle 1 missed the ball.
        game->paddle_2.score++;
        if (game->paddle_2.score == game->max_score) {
            game->ball = make_ball(2, true);
            return;
        }
        game->ball = make_ball(1, false);
        randomize_ghost_idle_offset(&game->ghost_1);
        randomize_ghost_idle_offset(&game->ghost_2);
        set_tonegen_tone(tonegen, SCORE_TONE);
    } else if (game->ball.rect.x > WINDOW_WIDTH) {
        // Paddle 2 missed the ball.
        game->paddle_1.score++;
        if (game->paddle_1.score == game->max_score) {
            game->ball = make_ball(1, true);
            return;
        }
        game->ball = make_ball(2, false);
        randomize_ghost_idle_offset(&game->ghost_1);
        randomize_ghost_idle_offset(&game->ghost_2);
        set_tonegen_tone(tonegen, SCORE_TONE);
    }
}

void check_round_over(struct game *game) {
    if (!game->round_over && (game->paddle_1.score == game->max_score ||
                              game->paddle_2.score == game->max_score)) {
        game->round_over = true;
        game->round_restart_timeout = SDL_GetTicks() + 6000; // in ms
    }
}

void restart_round(struct game *game) {
    game->paddle_1.score = 0;
    game->paddle_2.score = 0;
    game->ghost_1.inactive = false;
    game->ghost_2.inactive = false;
    randomize_ghost_speed(&game->ghost_1);
    randomize_ghost_speed(&game->ghost_2);
    game->ball = make_ball(rand_range(1, 2), false);
    game->round_over = false;
}

void check_round_restart_timeout(struct game *game) {
    if (game->round_over && SDL_GetTicks() >= game->round_restart_timeout) {
        restart_round(game);
    }
}

void render_score(SDL_Renderer *renderer, struct paddle paddle) {
    render_digits(
        renderer,
        (SDL_FPoint){
            .x =
                ((paddle.no == 1) ? (WINDOW_WIDTH / 2.0f) : WINDOW_WIDTH) - 100,
            .y = 50,
        },
        80, // height
        paddle.score);
}

void render_net(SDL_Renderer *renderer) {
    for (int y = 0; y < WINDOW_HEIGHT; y += NET_HEIGHT * 2) {
        SDL_RenderFillRectF(renderer,
                            &(SDL_FRect){
                                .x = (WINDOW_WIDTH - NET_WIDTH) / 2.0f,
                                .y = y,
                                .w = NET_WIDTH,
                                .h = NET_HEIGHT,
                            });
    }
}

void render_paddle(SDL_Renderer *renderer, struct game *game,
                   struct paddle paddle) {
    if (!game->round_over) {
        SDL_RenderFillRectF(renderer, &paddle.rect);
    }
}

void render_ball(SDL_Renderer *renderer, struct ball ball) {
    if (ball.served) {
        SDL_RenderFillRectF(renderer, &ball.rect);
    }
}

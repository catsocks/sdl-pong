#include <SDL.h>
#include <stdbool.h>
#include <time.h>

#include "font.h"
#include "math.h"
#include "tonegen.h"

#ifndef CHEATS_ENABLED
const bool CHEATS_ENABLED = false;
#endif

const bool AUDIO_ENABLED = true;
const bool JOYSTICKS_ENABLED = true;

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

const int ROUND_MAX_SCORE = 11;
const int ROUND_RESTART_DELAY = 6; // seconds

const int PADDLE_WIDTH = 10;
const int PADDLE_HEIGHT = 50;
const int PADDLE_X_MARGIN = 50;
const int PADDLE_SPEED = 500;     // px per second
const int PADDLE_MAX_SPEED = 700; // px per second, for joysticks

const int BALL_SIZE = 14;
const int BALL_INITIAL_SPEED = 360;  // px per second
const int BALL_MAX_SPEED = 500;      // px per second
const int BALL_SPEED_INCREMENT = 20; // px per second
const float BALL_MAX_SERVE_ANGLE = M_PI / 6;
const float BALL_MAX_BOUNCE_ANGLE = M_PI / 4;
const float BALL_SERVE_DELAY = 2; // seconds

const int SCORE_Y = 50;
const int SCORE_X_MARGIN = 100;
const int SCORE_HEIGHT = 80;

const int NET_WIDTH = 5;
const int NET_HEIGHT = 15;

const struct tone SCORE_TONE = {240, 510};
const struct tone PADDLE_HIT_TONE = {480, 35};
const struct tone WALL_HIT_TONE = {240, 20};

struct paddle {
    int no;
    float x, y;
    float velocity;
    int score;
};

struct ball {
    float x, y;
    float speed;
    SDL_FPoint velocity;
    float serve_timeout;
};

struct game {
    struct paddle paddle_1;
    struct paddle paddle_2;
    struct ball ball;
    float round_restart_timeout;
};

struct paddle make_paddle(int no);
void serve_ball(struct ball *ball, int paddle_no, bool round_over);
void check_paddle_controls(struct paddle *paddle_1, struct paddle *paddle_2,
                           SDL_Joystick *joystick_1, SDL_Joystick *joystick_2);
void update_paddle(struct paddle *paddle, double elapsed_time);
void update_ball(struct game *game, struct tonegen *tonegen, struct ball *ball,
                 double elapsed_time);
void bounce_ball_off_paddle(struct ball *ball, struct paddle *paddle);
bool paddle_intersects_ball(struct paddle paddle, struct ball ball);
void check_paddle_hit_ball(struct game *game, struct tonegen *tonegen);
void check_paddle_miss_ball(struct game *game, struct tonegen *tonegen);
void check_round_is_over(struct game *game);
void check_round_restart_timeout(struct game *game, double elapsed_time);
void render_paddle_score(SDL_Renderer *renderer, struct paddle paddle);
void render_net(SDL_Renderer *renderer);
void render_paddle(SDL_Renderer *renderer, struct game *game,
                   struct paddle paddle);
void render_ball(SDL_Renderer *renderer, struct ball ball);

int main(int argc, char *argv[]) {
    // SDL requires that main accept argc and argv when using MSVC or MinGW.
    (void)argc;
    (void)argv;

    // For choosing which paddle gets served the ball first and the vertical
    // position and angle of the ball every time it is served.
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    if (AUDIO_ENABLED) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Couldn't initialize the SDL audio subsystem: %s",
                         SDL_GetError());
        }
    }

    if (JOYSTICKS_ENABLED) {
        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Couldn't initialize the SDL joystick subsystem: %s",
                         SDL_GetError());
        }
    }

    // Try to open an audio device for playing mono signed 16-bit samples.
    SDL_AudioDeviceID audio_device_id =
        SDL_OpenAudioDevice(NULL, 0,
                            &(SDL_AudioSpec){.freq = TONEGEN_SAMPLING_RATE,
                                             .format = AUDIO_S16SYS,
                                             .channels = 1,
                                             .samples = 2048},
                            NULL, 0);
    if (audio_device_id == 0 && AUDIO_ENABLED) {
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

    // Assume at most two joysticks are detected, so that they'll be at index 0
    // and 1.
    SDL_Joystick *joystick_1 = SDL_JoystickOpen(0);
    SDL_Joystick *joystick_2 = SDL_JoystickOpen(1);

    SDL_ShowWindow(window);

    bool running = true;

    uint64_t counter_time = SDL_GetPerformanceCounter();

    struct game game = {
        .paddle_1 = make_paddle(1),
        .paddle_2 = make_paddle(2),
    };

    serve_ball(&game.ball, rand_range(1, 2), false);

    struct tonegen tonegen = {0};

    while (running) {
        uint64_t last_counter_time = counter_time;
        counter_time = SDL_GetPerformanceCounter();

        // For maintaining a constant game speed regardless of how fast the game
        // is running.
        double elapsed_time = (counter_time - last_counter_time) /
                              (double)SDL_GetPerformanceFrequency();

        // Poll events and handle quitting, toggling fullscreen and changing
        // the score of paddles for fun.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_F11:
                    if (SDL_GetWindowFlags(window) &
                        SDL_WINDOW_FULLSCREEN_DESKTOP) {
                        SDL_SetWindowFullscreen(window, 0);
                    } else {
                        SDL_SetWindowFullscreen(window,
                                                SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                    break;
                case SDLK_1:
                    if (CHEATS_ENABLED) {
                        game.paddle_1.score += 1;
                    }
                    break;
                case SDLK_2:
                    if (CHEATS_ENABLED) {
                        game.paddle_2.score += 1;
                    }
                    break;
                }
                break;
            }
        }

        // Begin to update the game state.
        check_paddle_controls(&game.paddle_1, &game.paddle_2, joystick_1,
                              joystick_2);

        update_paddle(&game.paddle_1, elapsed_time);
        update_paddle(&game.paddle_2, elapsed_time);
        update_ball(&game, &tonegen, &game.ball, elapsed_time);

        check_paddle_miss_ball(&game, &tonegen);
        check_paddle_hit_ball(&game, &tonegen);

        check_round_is_over(&game);
        check_round_restart_timeout(&game, elapsed_time);

        // Clear the renderer with black.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Begin drawing the game with white.
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        render_paddle_score(renderer, game.paddle_1);
        render_paddle_score(renderer, game.paddle_2);

        render_net(renderer);
        render_paddle(renderer, &game, game.paddle_1);
        render_paddle(renderer, &game, game.paddle_2);
        render_ball(renderer, game.ball);

        // Generate and queue audio.
        tonegen_generate(&tonegen);
        tonegen_queue(&tonegen, audio_device_id);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_CloseAudioDevice(audio_device_id);

    SDL_Quit();

    return EXIT_SUCCESS;
}

struct paddle make_paddle(int no) {
    return (struct paddle){
        .no = no,
        .x = (no == 1) ? PADDLE_X_MARGIN : WINDOW_WIDTH - PADDLE_X_MARGIN,
        .y = (WINDOW_HEIGHT - PADDLE_HEIGHT) / 2.0,
    };
}

// Place the ball on the side of the net of the paddle that it is to be served
// to and set its velocity so it goes at a random angle towards the paddle.
void serve_ball(struct ball *ball, int paddle_no, bool round_over) {
    ball->y = rand_range(0, WINDOW_HEIGHT - BALL_SIZE);

    ball->speed = BALL_INITIAL_SPEED;

    float angle = (-1 + (rand_double() * 2)) * BALL_MAX_SERVE_ANGLE;

    if (paddle_no == 1) {
        angle += M_PI;
        ball->x = ((WINDOW_WIDTH - BALL_SIZE) / 2.0) - NET_WIDTH * 2;
    } else {
        ball->x = ((WINDOW_WIDTH - BALL_SIZE) / 2.0) + NET_WIDTH * 2;
    }

    ball->velocity.x = cos(angle) * ball->speed;
    ball->velocity.y = -sin(angle) * ball->speed;

    if (!round_over) {
        ball->serve_timeout = BALL_SERVE_DELAY;
    }
}

// Set the vertical velocity of the paddles based on the vertical axis of the
// joysticks, or whether the keyboard keys are pressed, both can be used
// simultaneously and the keyboard is prioritized.
void check_paddle_controls(struct paddle *paddle_1, struct paddle *paddle_2,
                           SDL_Joystick *joystick_1, SDL_Joystick *joystick_2) {
    int16_t axis =
        SDL_JoystickGetAxis(joystick_1, 1); // 1 is commonly the y-axis
    paddle_1->velocity = PADDLE_MAX_SPEED * (axis / (float)INT16_MAX);

    axis = SDL_JoystickGetAxis(joystick_2, 1);
    paddle_2->velocity = PADDLE_MAX_SPEED * (axis / (float)INT16_MAX);

    const uint8_t *state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_W]) {
        paddle_1->velocity = -PADDLE_SPEED;
    } else if (state[SDL_SCANCODE_S]) {
        paddle_1->velocity = PADDLE_SPEED;
    }

    if (state[SDL_SCANCODE_UP]) {
        paddle_2->velocity = -PADDLE_SPEED;
    } else if (state[SDL_SCANCODE_DOWN]) {
        paddle_2->velocity = PADDLE_SPEED;
    }
}

void update_paddle(struct paddle *paddle, double elapsed_time) {
    paddle->y += paddle->velocity * elapsed_time;
    paddle->y = clamp(paddle->y, 0, WINDOW_HEIGHT - PADDLE_HEIGHT);
}

void update_ball(struct game *game, struct tonegen *tonegen, struct ball *ball,
                 double elapsed_time) {
    if (ball->serve_timeout > 0) {
        ball->serve_timeout -= elapsed_time;
        if (ball->serve_timeout < 0) {
            ball->serve_timeout = 0;
        }
        return;
    }

    ball->x += ball->velocity.x * elapsed_time;
    ball->y += ball->velocity.y * elapsed_time;

    // The ball will always bounce off vertical walls.
    if (ball->y < 0 || ball->y + BALL_SIZE > WINDOW_HEIGHT) {
        ball->velocity.y *= -1;
        if (game->round_restart_timeout == 0) {
            tonegen_set_tone(tonegen, WALL_HIT_TONE);
        }

        ball->y = clamp(ball->y, 0, WINDOW_HEIGHT - BALL_SIZE);
    }

    // The ball will only bounce off horizontal walls when the game is over.
    if (game->round_restart_timeout != 0) {
        if (ball->x < 0 || ball->x + BALL_SIZE > WINDOW_WIDTH) {
            ball->velocity.x *= -1;
            ball->x = clamp(ball->x, 0, WINDOW_WIDTH - BALL_SIZE);
        }
    }
}

void bounce_ball_off_paddle(struct ball *ball, struct paddle *paddle) {
    if (ball->speed < BALL_MAX_SPEED) {
        ball->speed += BALL_SPEED_INCREMENT;
    }

    // Relative to the center of the paddle and the ball.
    float intersect =
        paddle->y + (PADDLE_HEIGHT / 2.0) - ball->y - (BALL_SIZE / 2.0);

    float bounce_angle =
        (intersect / (PADDLE_HEIGHT / 2.0)) * BALL_MAX_BOUNCE_ANGLE;

    if (paddle->no == 1) {
        ball->x = paddle->x + PADDLE_WIDTH;

        ball->velocity.x = cos(bounce_angle) * ball->speed;
        ball->velocity.y = sin(bounce_angle) * ball->speed;
    } else {
        ball->x = paddle->x - BALL_SIZE;

        ball->velocity.x = cos(bounce_angle + M_PI) * ball->speed;
        ball->velocity.y = -sin(bounce_angle + M_PI) * ball->speed;
    }
}

// Return whether there is an intersection between the horizontal half of a
// paddle facing the net, and the ball.
bool paddle_intersects_ball(struct paddle paddle, struct ball ball) {
    bool y = paddle.y < ball.y + BALL_SIZE && paddle.y + PADDLE_HEIGHT > ball.y;
    if (paddle.no == 1) {
        return paddle.x + (PADDLE_WIDTH / 2.0) < ball.x + BALL_SIZE &&
               paddle.x + PADDLE_WIDTH > ball.x && y;
    }
    return paddle.x < ball.x + BALL_SIZE &&
           paddle.x + (PADDLE_WIDTH / 2.0) > ball.x && y;
}

// Return the ball in the other direction if it hit a paddle.
void check_paddle_hit_ball(struct game *game, struct tonegen *tonegen) {
    if (game->round_restart_timeout > 0) {
        return;
    }

    if (paddle_intersects_ball(game->paddle_1, game->ball)) {
        bounce_ball_off_paddle(&game->ball, &game->paddle_1);
    } else if (paddle_intersects_ball(game->paddle_2, game->ball)) {
        bounce_ball_off_paddle(&game->ball, &game->paddle_2);
    } else {
        return;
    }
    tonegen_set_tone(tonegen, PADDLE_HIT_TONE);
}

// Score a point and serve the ball when a paddle misses hitting the ball.
void check_paddle_miss_ball(struct game *game, struct tonegen *tonegen) {
    if (game->ball.x + BALL_SIZE < 0) {
        if (game->paddle_2.score == ROUND_MAX_SCORE - 1) {
            serve_ball(&game->ball, 2, true);
        } else {
            serve_ball(&game->ball, 1, false);
            tonegen_set_tone(tonegen, SCORE_TONE);
        }
        game->paddle_2.score++;
    } else if (game->ball.x > WINDOW_WIDTH) {
        if (game->paddle_1.score == ROUND_MAX_SCORE - 1) {
            serve_ball(&game->ball, 1, true);
        } else {
            serve_ball(&game->ball, 2, false);
            tonegen_set_tone(tonegen, SCORE_TONE);
        }
        game->paddle_1.score++;
    }
}

// Start the round restart timeout when one of the paddles reaches the max
// score.
void check_round_is_over(struct game *game) {
    if (game->round_restart_timeout == 0) {
        if (game->paddle_1.score == ROUND_MAX_SCORE ||
            game->paddle_2.score == ROUND_MAX_SCORE) {
            game->round_restart_timeout = ROUND_RESTART_DELAY;
        }
    }
}

// Update the round over timeout and restart the round when it finishes.
void check_round_restart_timeout(struct game *game, double elapsed_time) {
    if (game->round_restart_timeout > 0) {
        game->round_restart_timeout -= elapsed_time;
        if (game->round_restart_timeout <= 0) {
            game->round_restart_timeout = 0;
            game->paddle_1.score = 0;
            game->paddle_2.score = 0;
            serve_ball(&game->ball, rand_range(1, 2), false);
        }
    }
}

void render_paddle_score(SDL_Renderer *renderer, struct paddle paddle) {
    SDL_FPoint pos = {
        .x = (paddle.no == 1) ? (WINDOW_WIDTH / 2) - SCORE_X_MARGIN
                              : WINDOW_WIDTH - SCORE_X_MARGIN,
        .y = SCORE_Y,
    };
    render_number(renderer, pos, SCORE_HEIGHT, paddle.score);
}

void render_net(SDL_Renderer *renderer) {
    for (int y = 0; y < WINDOW_HEIGHT; y += NET_HEIGHT * 2) {
        SDL_RenderFillRect(renderer,
                           &(SDL_Rect){.x = (WINDOW_WIDTH - NET_WIDTH) / 2.0,
                                       .y = y,
                                       .w = NET_WIDTH,
                                       .h = NET_HEIGHT});
    }
}

void render_paddle(SDL_Renderer *renderer, struct game *game,
                   struct paddle paddle) {
    if (game->round_restart_timeout == 0) {
        SDL_RenderFillRectF(
            renderer,
            &(SDL_FRect){paddle.x, paddle.y, PADDLE_WIDTH, PADDLE_HEIGHT});
    }
}

void render_ball(SDL_Renderer *renderer, struct ball ball) {
    if (ball.serve_timeout == 0) {
        SDL_RenderFillRectF(renderer,
                            &(SDL_FRect){ball.x, ball.y, BALL_SIZE, BALL_SIZE});
    }
}

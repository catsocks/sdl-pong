#include <SDL.h>
#include <limits.h>
#include <stdbool.h>
#include <time.h>

#define PI 3.14159265358979323846

#define AUDIO_SAMPLING_RATE 44100
#define AUDIO_CHANNELS 1

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define ROUND_MAX_SCORE 11
#define ROUND_OVER_TIMEOUT 6 // seconds

#define PADDLE_WIDTH 10
#define PADDLE_HEIGHT 50
#define PADDLE_SCREEN_MARGIN_X 50
#define PADDLE_SPEED 400 // px per second

#define BALL_SIZE 14
#define BALL_SPEED_Y 300          // px per second
#define BALL_SPEED_INITIAL_X 300  // px per second
#define BALL_SPEED_MAX_X 540      // px per second
#define BALL_SPEED_INCREMENT_X 20 // px per second
#define BALL_SERVE_DELAY 2    // seconds

#define SCORE_Y 50
#define SCORE_SCREEN_MARGIN_X 150
#define SCORE_DIGIT_SCALE_FACTOR 10

#define NET_WIDTH 5
#define NET_HEIGHT 15

struct audio_clip {
    short *samples;
    size_t size;
};

struct audio {
    int device_id;
    struct audio_clip score;
    struct audio_clip paddle_hit;
    struct audio_clip bounce;
};

struct digits {
    int digit_width, digit_height;
    SDL_Texture *texture;
};

struct paddle {
    int no;
    float y;
    float velocity;
    SDL_Rect rect;
    int score;
};

struct ball {
    float x, y;
    float velocity_x, velocity_y;
    SDL_Rect rect;
    int last_paddle_no_hit;
    float serve_delay;
};

struct game {
    struct paddle paddle_1;
    struct paddle paddle_2;
    struct ball ball;
    float round_over_timeout;
};

int randrange(int min, int max) {
    return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

short sample_square_wave(int i, int freq, float amplitude) {
    if (sin(2 * PI * freq * i / (double)AUDIO_SAMPLING_RATE) >= 0) {
        return amplitude;
    }
    return -amplitude;
}

struct audio_clip make_tone(int freq, float duration, float amplitude) {
    int num_samples = duration * AUDIO_SAMPLING_RATE;
    size_t size = num_samples * sizeof(short);
    short *samples = malloc(size);

    amplitude *= SHRT_MAX;
    for (int i = 0; i < num_samples; i++) {
        samples[i] = sample_square_wave(i, freq, amplitude);
    }

    return (struct audio_clip){samples, size};
}

void queue_audio_clip(struct audio *audio, struct audio_clip *clip) {
    SDL_QueueAudio(audio->device_id, clip->samples, clip->size);
}

struct paddle make_paddle(int no) {
    struct paddle paddle = {.no = no,
                            .y = (WINDOW_HEIGHT - PADDLE_HEIGHT) / 2,
                            .rect = {
                                .w = PADDLE_WIDTH,
                                .h = PADDLE_HEIGHT,
                            }};
    paddle.rect.x = no == 1 ? PADDLE_SCREEN_MARGIN_X
                            : WINDOW_WIDTH - PADDLE_SCREEN_MARGIN_X;
    paddle.rect.y = roundf(paddle.y);
    return paddle;
}

void update_paddle(struct paddle *paddle, double elapsed_time) {
    paddle->y += paddle->velocity * elapsed_time;

    if (paddle->y < 0) {
        paddle->y = 0;
    } else if (paddle->y + PADDLE_HEIGHT > WINDOW_HEIGHT) {
        paddle->y = WINDOW_HEIGHT - PADDLE_HEIGHT;
    }

    paddle->rect.y = roundf(paddle->y);
}

struct ball make_ball() {
    return (struct ball){
        .rect =
            {
                .w = BALL_SIZE,
                .h = BALL_SIZE,
            },
    };
}

void serve_ball(struct ball *ball, int paddle_no, bool round_over) {
    if (paddle_no == 1) {
        ball->x = (WINDOW_WIDTH - BALL_SIZE) / 2 - NET_WIDTH * 2;
        ball->velocity_x = -BALL_SPEED_INITIAL_X;
    } else {
        ball->x = (WINDOW_WIDTH - BALL_SIZE) / 2 + NET_WIDTH * 2;
        ball->velocity_x = BALL_SPEED_INITIAL_X;
    }

    ball->y = randrange(0, WINDOW_HEIGHT - BALL_SIZE);

    if (!round_over) {
        ball->serve_delay = BALL_SERVE_DELAY;
    }

    ball->velocity_y = randrange(-BALL_SPEED_Y, BALL_SPEED_Y);

    ball->last_paddle_no_hit = 0;

    ball->rect.x = roundf(ball->x);
    ball->rect.y = roundf(ball->y);
}

void update_ball(struct game *game, struct audio *audio, struct ball *ball,
                 double elapsed_time) {
    if (ball->serve_delay > 0) {
        ball->serve_delay -= elapsed_time;
        if (ball->serve_delay < 0) {
            ball->serve_delay = 0;
        }
        return;
    }

    ball->x += ball->velocity_x * elapsed_time;
    ball->y += ball->velocity_y * elapsed_time;

    // The ball will always bounce off vertical edges.
    if (ball->y < 0 || ball->y + BALL_SIZE > WINDOW_HEIGHT) {
        ball->velocity_y *= -1;
        if (game->round_over_timeout == 0) {
            queue_audio_clip(audio, &audio->bounce);
        }
    }

    // The ball will only bounce off horizontal edges when the game is over.
    if (game->round_over_timeout != 0) {
        if (ball->x < 0 || ball->x + BALL_SIZE > WINDOW_WIDTH) {
            ball->velocity_x *= -1;
        }
    }

    ball->rect.x = roundf(ball->x);
    ball->rect.y = roundf(ball->y);
}

void check_input(struct paddle *paddle_1, struct paddle *paddle_2) {
    const uint8_t *state = SDL_GetKeyboardState(NULL);

    if (state[SDL_SCANCODE_W]) {
        paddle_1->velocity = -PADDLE_SPEED;
    } else if (state[SDL_SCANCODE_S]) {
        paddle_1->velocity = PADDLE_SPEED;
    } else {
        paddle_1->velocity = 0;
    }

    if (state[SDL_SCANCODE_UP]) {
        paddle_2->velocity = -PADDLE_SPEED;
    } else if (state[SDL_SCANCODE_DOWN]) {
        paddle_2->velocity = PADDLE_SPEED;
    } else {
        paddle_2->velocity = 0;
    }
}

void paddle_return_ball(struct ball *ball, struct paddle *peddle) {

    ball->last_paddle_no_hit = peddle->no;

    float collision_y = ((ball->y + ball->rect.h - peddle->y) /
                         (peddle->rect.h + ball->rect.h));

    ball->velocity_y += ((0.5 - collision_y) * BALL_SPEED_Y * 2);

    if (ball->velocity_y > BALL_SPEED_Y) {
        ball->velocity_y = BALL_SPEED_Y;
    } else if (ball->velocity_y < -BALL_SPEED_Y) {
        ball->velocity_y = -BALL_SPEED_Y;
    }

    ball->velocity_x *= -1;

    if (fabs(ball->velocity_x) < BALL_SPEED_MAX_X) {
        ball->velocity_x += ball->velocity_x > 0 ? BALL_SPEED_INCREMENT_X
                                                 : -BALL_SPEED_INCREMENT_X;
    }
}

void check_ball_paddle_hit(struct game *game, struct audio *audio) {
    if (game->round_over_timeout == 0) {
        if (SDL_HasIntersection(&game->paddle_1.rect, &game->ball.rect) &&
            game->ball.last_paddle_no_hit != 1) {
            paddle_return_ball(&game->ball, &game->paddle_1);
            queue_audio_clip(audio, &audio->paddle_hit);
        } else if (SDL_HasIntersection(&game->paddle_2.rect,
                                       &game->ball.rect) &&
                   game->ball.last_paddle_no_hit != 2) {
            paddle_return_ball(&game->ball, &game->paddle_2);
            queue_audio_clip(audio, &audio->paddle_hit);
        }
    }
}

void check_lost_ball(struct game *game, struct audio *audio) {
    if (game->ball.rect.x + game->ball.rect.w < 0) {
        if (game->paddle_2.score == 10) {
            serve_ball(&game->ball, 2, true);
        } else {
            serve_ball(&game->ball, 1, false);
            queue_audio_clip(audio, &audio->score);
        }
        game->paddle_2.score++;
    } else if (game->ball.rect.x > WINDOW_WIDTH) {
        if (game->paddle_1.score == 10) {
            serve_ball(&game->ball, 1, true);
        } else {
            serve_ball(&game->ball, 2, false);
            queue_audio_clip(audio, &audio->score);
        }
        game->paddle_1.score++;
    }
}

void update_game_timeout(struct game *game, double elapsed_time) {
    if (game->round_over_timeout > 0) {
        game->round_over_timeout -= elapsed_time;
        if (game->round_over_timeout <= 0) {
            game->paddle_1.score = 0;
            game->paddle_2.score = 0;
            serve_ball(&game->ball, randrange(1, 2), false);
            game->round_over_timeout = 0;
        }
    }
}

void check_round_over(struct game *game) {
    if (game->round_over_timeout == 0 && (game->paddle_1.score == ROUND_MAX_SCORE ||
                                          game->paddle_2.score == ROUND_MAX_SCORE)) {
        game->round_over_timeout = ROUND_OVER_TIMEOUT;
    }
}

// Render the score of a paddle using the digits in the digits image.
void render_paddle_score(SDL_Renderer *renderer, struct digits digits,
                         struct paddle paddle) {
    SDL_Rect src = {.w = digits.digit_width, .h = digits.digit_height};

    SDL_Rect dest = {.x = paddle.no == 1
                              ? WINDOW_WIDTH / 2 - SCORE_SCREEN_MARGIN_X
                              : WINDOW_WIDTH - SCORE_SCREEN_MARGIN_X,
                     .y = SCORE_Y,
                     .w = src.w * SCORE_DIGIT_SCALE_FACTOR,
                     .h = src.h * SCORE_DIGIT_SCALE_FACTOR};

    if (paddle.score == 0) {
        SDL_RenderCopy(renderer, digits.texture, &src, &dest);
        return;
    }

    int n = paddle.score;
    while (n != 0) {
        src.x = (n % 10) * src.w;
        SDL_RenderCopy(renderer, digits.texture, &src, &dest);
        dest.x -= SCORE_DIGIT_SCALE_FACTOR * src.w * 2;
        n /= 10;
    }
}

// Render the table tennis net in the middle of the screen with small filled
// rectangles.
void render_net(SDL_Renderer *renderer) {
    for (int y = 0; y < WINDOW_HEIGHT; y += NET_HEIGHT * 2) {
        SDL_RenderFillRect(renderer,
                           &(SDL_Rect){.x = (WINDOW_WIDTH - NET_WIDTH) / 2,
                                       .y = y,
                                       .w = NET_WIDTH,
                                       .h = NET_HEIGHT});
    }
}

// Render paddle as a filled rectangle.
void render_paddle(SDL_Renderer *renderer, struct game *game,
                   struct paddle paddle) {
    if (game->round_over_timeout == 0) {
        SDL_RenderFillRect(renderer, &(SDL_Rect){.x = paddle.rect.x,
                                                 .y = paddle.rect.y,
                                                 .w = paddle.rect.w,
                                                 .h = paddle.rect.h});
    }
}

// Render ball as a filled rectangle.
void render_ball(SDL_Renderer *renderer, struct ball ball) {
    if (ball.serve_delay == 0) {
        SDL_RenderFillRect(renderer, &(SDL_Rect){.x = ball.rect.x,
                                                 .y = ball.rect.y,
                                                 .w = ball.rect.w,
                                                 .h = ball.rect.h});
    }
}

int main() {
    // For choosing which paddle gets served the ball first and the vertical
    // position of the ball every time it appears.
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Initialize SDL: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_AudioDeviceID audio_device_id =
        SDL_OpenAudioDevice(NULL, 0,
                            &(SDL_AudioSpec){.freq = AUDIO_SAMPLING_RATE,
                                             .format = AUDIO_S16,
                                             .channels = AUDIO_CHANNELS,
                                             .samples = 2048},
                            NULL, SDL_AUDIO_ALLOW_FORMAT_CHANGE);

    struct audio audio = {
        .device_id = audio_device_id,
        .score = make_tone(240, 0.510, 0.025),
        .paddle_hit = make_tone(480, 0.035, 0.025),
        .bounce = make_tone(240, 0.020, 0.025),
    };

    // Unpause the audio device.
    SDL_PauseAudioDevice(audio_device_id, 0);

    // Create a hidden window so it may only be shown after the game is
    // initialized.
    SDL_Window *window = SDL_CreateWindow(
        "Pong", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH,
        WINDOW_HEIGHT, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Create window: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    // Synchronizing the renderer presentation to the screen's refresh rate
    // helps alleviate stuttering.
    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Create renderer: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    // So that handling different resolutions and mantaining the aspect
    // ratio in the given resolution isn't necessary.
    SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Load the digits for rendering the paddles score.
    SDL_Surface *digits_surf = SDL_LoadBMP("digits.bmp");
    if (digits_surf == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Load image: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    // Uncomment to make the black pixels transparent in case the game's
    // background is made to be a color other than black.
    // SDL_SetColorKey(digits_surf, SDL_TRUE,
    //                 SDL_MapRGB(digits_surf->format, 0, 0, 0));

    SDL_Texture *digits_tex =
        SDL_CreateTextureFromSurface(renderer, digits_surf);
    if (digits_tex == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Create texture: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    struct digits digits = {.digit_width = digits_surf->w / 10,
                            .digit_height = digits_surf->h,
                            .texture = digits_tex};

    // Show the window after the game is done initializing.
    SDL_ShowWindow(window);

    bool running = true;

    uint64_t counter_time = SDL_GetPerformanceCounter();

    struct game game = {.paddle_1 = make_paddle(1),
                        .paddle_2 = make_paddle(2),
                        .ball = make_ball()};

    serve_ball(&game.ball, randrange(1, 2), false);

    while (running) {
        // Calculate the difference of time between the last frame and the
        // current frame for the purpose of mantaining a consistent game
        // speed regardless of how fast or slow frames are being drawn.
        uint64_t last_counter_time = counter_time;
        counter_time = SDL_GetPerformanceCounter();
        double elapsed_time = (counter_time - last_counter_time) /
                              (double)SDL_GetPerformanceFrequency();

        // Poll events and handle quitting, toggling fullscreen and changing
        // the score when debugging.
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
#ifdef CHEATS
                case SDLK_1:
                    game.paddle_1.score += 1;
                    break;
                case SDLK_2:
                    game.paddle_2.score += 1;
                    break;
#endif
                }
                break;
            }
        }

        // Begin to update the game state.
        check_input(&game.paddle_1, &game.paddle_2);

        update_paddle(&game.paddle_1, elapsed_time);
        update_paddle(&game.paddle_2, elapsed_time);
        update_ball(&game, &audio, &game.ball, elapsed_time);

        update_game_timeout(&game, elapsed_time);

        check_lost_ball(&game, &audio);
        check_ball_paddle_hit(&game, &audio);

        check_round_over(&game);

        // Clear the renderer with black.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        render_paddle_score(renderer, digits, game.paddle_1);
        render_paddle_score(renderer, digits, game.paddle_2);

        // Begin drawing the game with white.
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        render_net(renderer);
        render_paddle(renderer, &game, game.paddle_1);
        render_paddle(renderer, &game, game.paddle_2);
        render_ball(renderer, game.ball);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(digits_tex);
    SDL_FreeSurface(digits_surf);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    free(audio.score.samples);
    free(audio.paddle_hit.samples);
    free(audio.bounce.samples);

    SDL_CloseAudioDevice(audio_device_id);

    SDL_Quit();

    return EXIT_SUCCESS;
}
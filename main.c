#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const int AUDIO_SAMPLING_RATE = 44100;
const float AUDIO_AMPLITUDE = 0.025 * INT16_MAX; // volume

const bool AUDIO_ENABLED = true;
const bool JOYSTICKS_ENABLED = true;

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

// Assumes that at most 2 joysticks are detected.
const int JOYSTICK_1_IDX = 0;
const int JOYSTICK_2_IDX = 1;

const int JOYSTICK_1_AXIS_IDX = 1; // 1 is commonly the y-axis
const int JOYSTICK_2_AXIS_IDX = 1;

const int PADDLE_1_UP_KEY = SDL_SCANCODE_W;
const int PADDLE_1_DOWN_KEY = SDL_SCANCODE_S;
const int PADDLE_2_UP_KEY = SDL_SCANCODE_UP;
const int PADDLE_2_DOWN_KEY = SDL_SCANCODE_DOWN;

const int ROUND_MAX_SCORE = 11;
const int ROUND_OVER_TIMEOUT = 6; // seconds

const bool CHEATS_ENABLED = false;

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
const int SCORE_X_MARGIN = 150;
const int SCORE_DIGIT_SCALE = 10;

const int NET_WIDTH = 5;
const int NET_HEIGHT = 15;

struct audio_clip {
    int16_t *samples;
    size_t samples_size;
};

struct audio {
    SDL_AudioDeviceID device_id;
    struct audio_clip score;
    struct audio_clip paddle_hit;
    struct audio_clip bounce;
};

struct digits_image {
    SDL_Texture *texture;
    SDL_Rect digit_size;
};

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
    float serve_delay;
};

struct game {
    struct paddle paddle_1;
    struct paddle paddle_2;
    struct ball ball;
    float round_over_timeout;
};

int clamp(int x, int min, int max) {
    return x < min ? min : x > max ? max : x;
}

int rand_range(int min, int max) {
    return min + (rand() / ((RAND_MAX / (max - min + 1)) + 1));
}

double rand_double() {
    return rand() / (double)RAND_MAX;
}

struct audio_clip make_square_wave(int freq, float duration) {
    int length = duration * AUDIO_SAMPLING_RATE;
    size_t size = length * sizeof(int16_t);
    int16_t *samples = malloc(size);

    for (int i = 0; i < length; i++) {
        if (sin(2 * M_PI * freq * (i / (double)AUDIO_SAMPLING_RATE)) >= 0) {
            samples[i] = AUDIO_AMPLITUDE;
        } else {
            samples[i] = -AUDIO_AMPLITUDE;
        }
    }

    return (struct audio_clip){samples, size};
}

void queue_audio_clip(struct audio *audio, struct audio_clip *clip) {
    SDL_QueueAudio(audio->device_id, clip->samples, clip->samples_size);
}

struct paddle make_paddle(int no) {
    return (struct paddle){
        .no = no,
        .x = no == 1 ? PADDLE_X_MARGIN : WINDOW_WIDTH - PADDLE_X_MARGIN,
        .y = (WINDOW_HEIGHT - PADDLE_HEIGHT) / 2,
    };
}

// Position the ball on one of the sides of the net and change its velocity
// based on which paddle it is being served to.
void serve_ball(struct ball *ball, int paddle_no, bool round_over) {
    ball->y = rand_range(0, WINDOW_HEIGHT - BALL_SIZE);

    ball->speed = BALL_INITIAL_SPEED;

    float angle = (-1 + (rand_double() * 2)) * BALL_MAX_SERVE_ANGLE;

    if (paddle_no == 1) {
        angle += M_PI;
        ball->x = ((WINDOW_WIDTH - BALL_SIZE) / 2) - NET_WIDTH * 2;
    } else {
        ball->x = ((WINDOW_WIDTH - BALL_SIZE) / 2) + NET_WIDTH * 2;
    }

    ball->velocity.x = cos(angle) * ball->speed;
    ball->velocity.y = -sin(angle) * ball->speed;

    if (!round_over) {
        ball->serve_delay = BALL_SERVE_DELAY;
    }
}

// Set the vertical velocity of the paddles based on the state of the paddle
// controls.
// The keyboard and joystick controls can be used simultaneously.
void check_paddle_controls(struct paddle *paddle_1, struct paddle *paddle_2,
                           SDL_Joystick *joystick_1, SDL_Joystick *joystick_2) {
    const uint8_t *state = SDL_GetKeyboardState(NULL);

    int16_t axis = SDL_JoystickGetAxis(joystick_1, JOYSTICK_1_AXIS_IDX);
    paddle_1->velocity = PADDLE_MAX_SPEED * (axis / (float)INT16_MAX);
    axis = SDL_JoystickGetAxis(joystick_2, JOYSTICK_2_AXIS_IDX);
    paddle_2->velocity = PADDLE_MAX_SPEED * (axis / (float)INT16_MAX);

    if (state[PADDLE_1_UP_KEY]) {
        paddle_1->velocity = -PADDLE_SPEED;
    } else if (state[PADDLE_1_DOWN_KEY]) {
        paddle_1->velocity = PADDLE_SPEED;
    }

    if (state[PADDLE_2_UP_KEY]) {
        paddle_2->velocity = -PADDLE_SPEED;
    } else if (state[PADDLE_2_DOWN_KEY]) {
        paddle_2->velocity = PADDLE_SPEED;
    }
}

void update_paddle(struct paddle *paddle, double elapsed_time) {
    paddle->y += paddle->velocity * elapsed_time;
    paddle->y = clamp(paddle->y, 0, WINDOW_HEIGHT - PADDLE_HEIGHT);
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

    ball->x += ball->velocity.x * elapsed_time;
    ball->y += ball->velocity.y * elapsed_time;

    // The ball will always bounce off vertical walls.
    if (ball->y < 0 || ball->y + BALL_SIZE > WINDOW_HEIGHT) {
        ball->velocity.y *= -1;
        if (game->round_over_timeout == 0) {
            queue_audio_clip(audio, &audio->bounce);
        }

        ball->y = clamp(ball->y, 0, WINDOW_HEIGHT - BALL_SIZE);
    }

    // The ball will only bounce off horizontal walls when the game is over.
    if (game->round_over_timeout != 0) {
        if (ball->x < 0 || ball->x + BALL_SIZE > WINDOW_WIDTH) {
            ball->velocity.x *= -1;
        }

        ball->x = clamp(ball->x, 0, WINDOW_WIDTH - BALL_SIZE);
    }
}

// Set the velocity of the ball based on which paddle it is being returned to
// and where it hit the given paddle.
void return_ball(struct ball *ball, struct paddle *paddle) {
    if (ball->speed < BALL_MAX_SPEED) {
        ball->speed += BALL_SPEED_INCREMENT;
    }

    // The vertical intersection relative to the center of both the paddle and
    // ball.
    float intersect =
        paddle->y + (PADDLE_HEIGHT / 2) - ball->y - (BALL_SIZE / 2);

    float bounce_angle =
        (intersect / (PADDLE_HEIGHT / 2)) * BALL_MAX_BOUNCE_ANGLE;

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

// Return whether there is an intersection between a paddle and the ball.
bool paddle_intersects_ball(struct paddle paddle, struct ball ball) {
    return paddle.x < ball.x + BALL_SIZE && paddle.x + PADDLE_WIDTH > ball.x &&
           paddle.y < ball.y + BALL_SIZE && paddle.y + PADDLE_HEIGHT > ball.y;
}

// Return the ball in the other paddle's angle if it hit a paddle.
void check_paddle_hit_ball(struct game *game, struct audio *audio) {
    if (game->round_over_timeout > 0) {
        return;
    }

    if (paddle_intersects_ball(game->paddle_1, game->ball)) {
        return_ball(&game->ball, &game->paddle_1);
        queue_audio_clip(audio, &audio->paddle_hit);
    } else if (paddle_intersects_ball(game->paddle_2, game->ball)) {
        return_ball(&game->ball, &game->paddle_2);
        queue_audio_clip(audio, &audio->paddle_hit);
    }
}

// Score a point for a paddle if the other paddle missed returning the ball.
void check_paddle_miss_ball(struct game *game, struct audio *audio) {
    if (game->ball.x + BALL_SIZE < 0) {
        if (game->paddle_2.score == ROUND_MAX_SCORE - 1) {
            serve_ball(&game->ball, 2, true);
        } else {
            serve_ball(&game->ball, 1, false);
            queue_audio_clip(audio, &audio->score);
        }
        game->paddle_2.score++;
    } else if (game->ball.x > WINDOW_WIDTH) {
        if (game->paddle_1.score == ROUND_MAX_SCORE - 1) {
            serve_ball(&game->ball, 1, true);
        } else {
            serve_ball(&game->ball, 2, false);
            queue_audio_clip(audio, &audio->score);
        }
        game->paddle_1.score++;
    }
}

// Start the round over timeout when one of the paddles reaches the max score,
// update the round over timeout and restart the round when the timeout is over.
void check_round_over(struct game *game, double elapsed_time) {
    if (game->round_over_timeout == 0) {
        if (game->paddle_1.score == ROUND_MAX_SCORE ||
            game->paddle_2.score == ROUND_MAX_SCORE) {
            game->round_over_timeout = ROUND_OVER_TIMEOUT;
        }
    } else {
        game->round_over_timeout -= elapsed_time;
        if (game->round_over_timeout <= 0) {
            game->paddle_1.score = 0;
            game->paddle_2.score = 0;
            serve_ball(&game->ball, rand_range(1, 2), false);
            game->round_over_timeout = 0;
        }
    }
}

// Render the score of a paddle using the digits in the digits image.
void render_paddle_score(SDL_Renderer *renderer, struct digits_image digits,
                         struct paddle paddle) {
    SDL_Rect src = digits.digit_size;

    SDL_Rect dest = {
        .x = paddle.no == 1 ? (WINDOW_WIDTH / 2) - SCORE_X_MARGIN
                            : WINDOW_WIDTH - SCORE_X_MARGIN,
        .y = SCORE_Y,
        .w = src.w * SCORE_DIGIT_SCALE,
        .h = src.h * SCORE_DIGIT_SCALE,
    };

    if (paddle.score == 0) {
        SDL_RenderCopy(renderer, digits.texture, &src, &dest);
        return;
    }

    int n = paddle.score;
    while (n != 0) {
        src.x = (n % 10) * src.w;
        SDL_RenderCopy(renderer, digits.texture, &src, &dest);
        dest.x -= SCORE_DIGIT_SCALE * src.w * 2;
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
        SDL_RenderFillRectF(
            renderer,
            &(SDL_FRect){paddle.x, paddle.y, PADDLE_WIDTH, PADDLE_HEIGHT});
    }
}

// Render ball as a filled rectangle.
void render_ball(SDL_Renderer *renderer, struct ball ball) {
    if (ball.serve_delay == 0) {
        SDL_RenderFillRectF(renderer,
                            &(SDL_FRect){ball.x, ball.y, BALL_SIZE, BALL_SIZE});
    }
}

int main() {
    // Generate square waves to be used as sound effects for when a paddles
    // scores a point, a paddle hits the ball and when the ball bounces off the
    // vertical screen edges.
    struct audio audio = {
        .score = make_square_wave(240, 0.510),
        .paddle_hit = make_square_wave(480, 0.035),
        .bounce = make_square_wave(240, 0.020),
    };

    // For choosing which paddle gets served the ball first and the vertical
    // position and angle of the ball every time it is served.
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Initialize SDL: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    // The audio subsystem is not required.
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0 && AUDIO_ENABLED) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Initialize SDL audio subsystem: %s", SDL_GetError());
    }

    // The joystick subsystem is not required.
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0 && JOYSTICKS_ENABLED) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Initialize SDL joystick subsystem: %s", SDL_GetError());
    }

    // Open an audio device for playing signed 16-bit mono samples and ignore if
    // no device can be opened.
    audio.device_id =
        SDL_OpenAudioDevice(NULL, 0,
                            &(SDL_AudioSpec){.freq = AUDIO_SAMPLING_RATE,
                                             .format = AUDIO_S16SYS,
                                             .channels = 1,
                                             .samples = 2048},
                            NULL, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (audio.device_id == 0 && AUDIO_ENABLED) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Open audio device: %s",
                     SDL_GetError());
    }

    // Unpause the audio device which is paused by default.
    SDL_PauseAudioDevice(audio.device_id, 0);

    // Create a hidden window so it may only be shown after the game is
    // initialized.
    SDL_Window *window = SDL_CreateWindow(
        "Tennis", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
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

    // Try to open and get the identifier for two joysticks, ignoring if any
    // error occurs.
    SDL_Joystick *joystick_1 = SDL_JoystickOpen(JOYSTICK_1_IDX);
    SDL_Joystick *joystick_2 = SDL_JoystickOpen(JOYSTICK_2_IDX);

    // Load the digits for rendering the paddles score.
    SDL_Surface *digits_surf = SDL_LoadBMP("digits.bmp");
    if (digits_surf == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Load image: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    // Uncomment to make the black pixels transparent in case the game's
    // background is set to a color other than black.
    // SDL_SetColorKey(digits_surf, SDL_TRUE,
    //                 SDL_MapRGB(digits_surf->format, 0, 0, 0));

    SDL_Texture *digits_tex =
        SDL_CreateTextureFromSurface(renderer, digits_surf);
    if (digits_tex == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Create texture: %s",
                     SDL_GetError());
        return EXIT_FAILURE;
    }

    struct digits_image digits = {
        .digit_size = {.w = digits_surf->w / 10, .h = digits_surf->h},
        .texture = digits_tex,
    };

    // Show the window after the game is done initializing.
    SDL_ShowWindow(window);

    bool running = true;

    uint64_t counter_time = SDL_GetPerformanceCounter();

    struct game game = {
        .paddle_1 = make_paddle(1),
        .paddle_2 = make_paddle(2),
    };

    serve_ball(&game.ball, rand_range(1, 2), false);

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
        update_ball(&game, &audio, &game.ball, elapsed_time);

        check_paddle_miss_ball(&game, &audio);
        check_paddle_hit_ball(&game, &audio);

        check_round_over(&game, elapsed_time);

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

    SDL_CloseAudioDevice(audio.device_id);

    SDL_Quit();

    free(audio.score.samples);
    free(audio.paddle_hit.samples);
    free(audio.bounce.samples);

    return EXIT_SUCCESS;
}

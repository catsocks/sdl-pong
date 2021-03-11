#include "game.h"

const int LOGICAL_WIDTH = 800;
const int LOGICAL_HEIGHT = 600;

const int NET_WIDTH = 5;
const int NET_HEIGHT = 15;

struct game make_game(bool cheats_enabled) {
    struct game game = {0};
    game.cheats_enabled = cheats_enabled;
    game.paddle_1 = make_paddle(1);
    game.paddle_2 = make_paddle(2);
    game.ghost_1 = make_ghost(game.difficulty);
    game.ghost_2 = make_ghost(game.difficulty);
    game.ball = make_ball(rand_range(1, 2), false, game.time);
    game.ghost_ball = make_ghost_ball(game.ball, game.difficulty);
    game.max_score = 11;
    return game;
}

struct paddle make_paddle(int no) {
    struct paddle paddle = {0};
    paddle.no = no;
    paddle.rect.w = 10.0f;
    paddle.rect.h = 50.0f;
    float margin = 50.0f;
    paddle.rect.x = (paddle.no == 1) ? margin : LOGICAL_WIDTH - margin;
    paddle.rect.y = (LOGICAL_HEIGHT - paddle.rect.h) / 2.0f;
    paddle.max_speed = 500.0f;
    return paddle;
}

struct ghost make_ghost(float difficulty) {
    struct ghost ghost = {0};
    ghost.active = true;
    set_ghost_speed(&ghost, difficulty);
    set_ghost_bias(&ghost);
    return ghost;
}

// Return a ball that is on the side of the net of the given paddle with its
// velocity set so it moves at a random angle towards the paddle.
struct ball make_ball(int paddle_no, bool round_over, double time) {
    struct ball ball = {0};

    int size = 14;
    ball.rect.w = size;
    ball.rect.h = size;
    ball.rect.x = (LOGICAL_WIDTH - ball.rect.w) / 2.0f;
    ball.rect.x += NET_WIDTH * ((paddle_no == 1) ? -2.0f : 2.0f);
    ball.rect.y = frand_range(0.0f, LOGICAL_HEIGHT - ball.rect.h);

    float angle = frand_range(-1.0f, 1.0f) * (M_PI / 6.0f);
    if (paddle_no == 1) {
        angle += M_PI;
    }
    float speed = 360.0f;
    ball.velocity.x = cosf(angle) * speed;
    ball.velocity.y = -sinf(angle) * speed;

    if (!round_over) {
        ball.serve_time = time + 2.0;
    }

    return ball;
}

struct ball make_ghost_ball(struct ball ball, float difficulty) {
    float angle = atan2f(ball.velocity.y, ball.velocity.x);
    float speed = sqrtf((ball.velocity.y * ball.velocity.y) +
                        (ball.velocity.x * ball.velocity.x));
    float max_speed_difference = 80.0f * (1.0f - difficulty);
    speed += frand_range(-max_speed_difference, max_speed_difference);
    ball.velocity.x = cosf(angle) * speed;
    ball.velocity.y = sinf(angle) * speed;
    return ball;
}

void check_paddle_controls(struct paddle *paddle, struct ghost *ghost,
                           struct player_input *input) {
    float velocity = 0;
    if (input->finger_down) {
        float target = input->finger_y - (paddle->rect.h / 2.0f);
        float distance = fabsf(target - paddle->rect.y);
        float cutoff = paddle->rect.h / 4.0f;
        float distance_factor = fmin(distance, cutoff) / cutoff;
        float speed = paddle->max_speed * distance_factor;
        velocity = sign(target - paddle->rect.y) * speed;
    }

    if (SDL_GameControllerGetButton(
            input->controller, SDL_CONTROLLER_BUTTON_DPAD_UP) == SDL_PRESSED) {
        velocity = -paddle->max_speed;
    } else if (SDL_GameControllerGetButton(input->controller,
                                           SDL_CONTROLLER_BUTTON_DPAD_DOWN) ==
               SDL_PRESSED) {
        velocity = paddle->max_speed;
    }

    SDL_Scancode up_key = SDL_SCANCODE_W;
    SDL_Scancode down_key = SDL_SCANCODE_S;
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
        if (ghost->active) {
            paddle->velocity = ghost->velocity;
        } else {
            paddle->velocity = 0;
        }
        return;
    }
    paddle->velocity = velocity;
    ghost->active = false;
    input->last_input_timestamp = SDL_GetTicks();
    return;
}

void check_input_inactivity(struct player_input input, struct ghost *ghost) {
    int timeout = 10000; // in ms
    if (SDL_GetTicks() > input.last_input_timestamp + timeout) {
        ghost->active = true;
    }
}

void set_ghost_velocity(struct ghost *ghost, struct paddle paddle,
                        struct ball ball) {
    if (!ghost->active) {
        return;
    }

    float target =
        ((LOGICAL_HEIGHT - paddle.rect.h) / 2.0f) + ghost->idle_offset;
    if (ball.served) {
        float bias = (paddle.rect.h / 2.0f) * ghost->bias;
        target = ball.rect.y - ((paddle.rect.h - ball.rect.h) / 2.0f) + bias;
    }

    float ball_distance = fabsf(ball.rect.x - paddle.rect.x);
    float cutoff = LOGICAL_WIDTH / 1.1f;
    float ball_dist_factor = 1.0f - (fminf(ball_distance, cutoff) / cutoff);

    float target_distance = fabsf(target - paddle.rect.y);
    cutoff = paddle.rect.h / 2.0f;
    float target_dist_factor = fminf(target_distance, cutoff) / cutoff;

    float ball_dir_factor = 1.0f;
    if ((ball.velocity.x > 0.0f && paddle.no == 1) ||
        (ball.velocity.x < 0.0f && paddle.no == 2)) {
        ball_dir_factor = 0.5f;
    }

    float speed = paddle.max_speed * ghost->speed * ball_dist_factor *
                  target_dist_factor * ball_dir_factor;
    ghost->velocity = sign(target - paddle.rect.y) * speed;
}

void update_paddle(struct paddle *paddle, double dt) {
    paddle->rect.y += paddle->velocity * dt;
    paddle->rect.y =
        clamp(paddle->rect.y, 0.0f, LOGICAL_HEIGHT - paddle->rect.h);
}

void set_ghost_speed(struct ghost *ghost, float difficulty) {
    ghost->speed = fminf(0.70f + (difficulty * 20.0f), 0.90f);
}

void set_ghost_bias(struct ghost *ghost) {
    ghost->bias = frand_range(-1.0f, 1.0f);
}

void set_ghost_idle_offset(struct ghost *ghost) {
    int max_distance = LOGICAL_HEIGHT / 8;
    ghost->idle_offset = rand_range(-max_distance, max_distance);
}

void update_ball(struct ball *ball, double dt, double t) {
    // The ball will always bounce off vertical walls.
    if (ball->rect.y < 0.0f || ball->rect.y + ball->rect.h > LOGICAL_HEIGHT) {
        ball->velocity.y *= -1.0f;
        ball->rect.y = clamp(ball->rect.y, 0.0f, LOGICAL_HEIGHT - ball->rect.h);
    }

    // The ball will only bounce off horizontal walls when the round is over.
    if (ball->horizontal_bounce) {
        if (ball->rect.x < 0.0f ||
            ball->rect.x + ball->rect.h > LOGICAL_WIDTH) {
            ball->velocity.x *= -1.0f;
            ball->rect.x =
                clamp(ball->rect.x, 0.0f, LOGICAL_WIDTH - ball->rect.w);
        }
    }

    if (ball->served) {
        ball->rect.x += ball->velocity.x * dt;
        ball->rect.y += ball->velocity.y * dt;
    } else if (t >= ball->serve_time) {
        ball->served = true;
    }
}

void check_ball_hit_wall(struct game *game) {
    struct ball *ball = &game->ball;

    // The ball will always bounce off vertical walls.
    if (!game->round_over) {
        if (ball->rect.y < 0.0f ||
            ball->rect.y + ball->rect.h > LOGICAL_HEIGHT) {
            game->events.ball_hit_wall = true;
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
        (intersect / (paddle->rect.h / 2.0f)) * max_bounce_angle;

    // The length of the velocity vector.
    float speed = sqrtf((ball->velocity.y * ball->velocity.y) +
                        (ball->velocity.x * ball->velocity.x));

    // Increment speed if it hasn't reached the limit.
    if (speed < 540.0f) {
        speed += 10.0f;
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

void check_paddle_hit_ball(struct game *game) {
    if (!game->round_over) {
        if (paddle_intersects_ball(game->paddle_1, game->ball)) {
            bounce_ball_off_paddle(&game->ball, &game->paddle_1);
            game->ghost_ball = make_ghost_ball(game->ball, game->difficulty);
            set_ghost_bias(&game->ghost_2);
        } else if (paddle_intersects_ball(game->paddle_2, game->ball)) {
            bounce_ball_off_paddle(&game->ball, &game->paddle_2);
            game->ghost_ball = make_ghost_ball(game->ball, game->difficulty);
            set_ghost_bias(&game->ghost_1);
        } else {
            return;
        }
        game->events.ball_hit_paddle = true;
    }
}

void check_paddle_missed_ball(struct game *game) {
    if (game->ball.rect.x + game->ball.rect.w < 0) {
        // Paddle 1 missed the ball.
        game->paddle_2.score++;
        if (game->paddle_2.score == game->max_score) {
            game->ball = make_ball(2, true, game->time);
            return;
        }
        game->ball = make_ball(1, false, game->time);
    } else if (game->ball.rect.x > LOGICAL_WIDTH) {
        // Paddle 2 missed the ball.
        game->paddle_1.score++;
        if (game->paddle_1.score == game->max_score) {
            game->ball = make_ball(1, true, game->time);
            return;
        }
        game->ball = make_ball(2, false, game->time);
    } else {
        return;
    }

    game->ghost_ball = make_ghost_ball(game->ball, game->difficulty);
    set_ghost_idle_offset(&game->ghost_1);
    set_ghost_idle_offset(&game->ghost_2);
    game->events.paddle_missed_ball = true;
}

void check_game_events(struct game *game, struct tonegen *tonegen) {
    if (game->events.paddle_missed_ball) {
        set_tonegen_tone(tonegen, 240, 510);
    } else if (game->events.ball_hit_paddle) {
        set_tonegen_tone(tonegen, 480, 35);
    } else if (game->events.ball_hit_wall) {
        set_tonegen_tone(tonegen, 240, 20);
    }

    game->events = (struct events){0};
}

void check_round_over(struct game *game) {
    if (!game->round_over && (game->paddle_1.score == game->max_score ||
                              game->paddle_2.score == game->max_score)) {
        game->ball.horizontal_bounce = true;
        game->round_over = true;
        game->round_restart_time = game->time + 6.0;
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Round over: %d-%d",
                     game->paddle_1.score, game->paddle_2.score);
    }
}

void restart_round(struct game *game) {
    if (game->round_over) {
        if ((game->paddle_1.score == game->max_score &&
             !game->ghost_1.active) ||
            (game->paddle_2.score == game->max_score &&
             !game->ghost_2.active) ||
            (game->ghost_2.active && game->ghost_2.active)) {
            // Only increase the difficulty of the game if a paddle controlled
            // by a player wins the round or if the ghosts played against each
            // other.
            game->difficulty = fminf(game->difficulty + 0.2f, 1.0f);
        }
    }
    game->paddle_1.score = 0;
    game->paddle_2.score = 0;
    set_ghost_speed(&game->ghost_1, game->difficulty);
    set_ghost_speed(&game->ghost_2, game->difficulty);
    game->ball = make_ball(rand_range(1, 2), false, game->time);
    game->ghost_ball = make_ghost_ball(game->ball, game->difficulty);
    game->round_over = false;
}

void check_round_restart_timeout(struct game *game) {
    if (game->round_over && game->time >= game->round_restart_time) {
        restart_round(game);
    }
}

void render_score(struct renderer_wrapper renderer_wrapper,
                  struct paddle paddle) {
    render_digits(
        renderer_wrapper,
        (SDL_FPoint){
            .x = ((paddle.no == 1) ? (LOGICAL_WIDTH / 2.0f) : LOGICAL_WIDTH) -
                 100.0f,
            .y = 50.0f,
        },
        80, // height
        paddle.score);
}

void render_net(struct renderer_wrapper renderer_wrapper) {
    for (int y = 0; y < LOGICAL_HEIGHT; y += NET_HEIGHT * 2) {
        SDL_FRect rect = {
            .x = (LOGICAL_WIDTH - NET_WIDTH) / 2.0f,
            .y = y,
            .w = NET_WIDTH,
            .h = NET_HEIGHT,
        };
        rect = scale_frect(renderer_wrapper, rect);
        SDL_RenderFillRectF(renderer_wrapper.renderer, &rect);
    }
}

void render_paddle(struct renderer_wrapper renderer_wrapper, struct game *game,
                   struct paddle paddle) {
    if (!game->round_over) {
        SDL_FRect rect = scale_frect(renderer_wrapper, paddle.rect);
        SDL_RenderFillRectF(renderer_wrapper.renderer, &rect);
    }
}

void render_ball(struct renderer_wrapper renderer_wrapper, struct ball ball) {
    if (ball.served) {
        SDL_FRect rect = scale_frect(renderer_wrapper, ball.rect);
        SDL_RenderFillRectF(renderer_wrapper.renderer, &rect);
    }
}

void debug_render_ghost_ball(struct renderer_wrapper renderer_wrapper,
                             struct ball ball) {
    SDL_Color c;
    SDL_GetRenderDrawColor(renderer_wrapper.renderer, &c.r, &c.g, &c.b, &c.a);
    SDL_SetRenderDrawColor(renderer_wrapper.renderer, 0, 255, 0, 255); // green
    render_ball(renderer_wrapper, ball);
    SDL_SetRenderDrawColor(renderer_wrapper.renderer, c.r, c.g, c.b, c.a);
}

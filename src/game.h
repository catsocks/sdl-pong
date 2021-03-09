#pragma once
#include <SDL.h>
#include <stdbool.h>

#include "digits.h"
#include "math.h"
#include "renderer.h"
#include "tonegen.h"

extern const int LOGICAL_WIDTH;
extern const int LOGICAL_HEIGHT;

struct ghost {
    int idle_offset;
    float speed;
    float bias;
    bool inactive;
    float velocity;
};

struct paddle {
    int no;
    SDL_FRect rect;
    float velocity;
    float max_speed;
    int score;
};

struct ball {
    SDL_FRect rect;
    SDL_FPoint velocity;
    bool served;
    uint32_t serve_time;
    bool horizontal_bounce;
};

struct events {
    bool paddle_missed_ball;
    bool ball_hit_paddle;
    bool ball_hit_wall;
};

struct game {
    bool cheats_enabled;
    struct paddle paddle_1;
    struct paddle paddle_2;
    struct ghost ghost_1;
    struct ghost ghost_2;
    struct ball ball;
    struct ball ghost_ball;
    int max_score;
    bool paused;
    double time;
    bool debug_mode;
    bool round_over;
    uint32_t round_restart_time;
    struct events events;
};

struct player_input {
    SDL_GameController *controller;
    SDL_TouchID touch_id;
    SDL_FingerID finger_id;
    int finger_y;
    bool finger_down;
    uint32_t last_input_timestamp;
};

struct game make_game(bool cheats_enabled);
struct paddle make_paddle(int no);
struct ghost make_ghost();
struct ball make_ball(int paddle_no, bool round_over, double t);
struct ball make_ghost_ball(struct ball ball);
void check_paddle_controls(struct paddle *paddle, struct ghost *ghost,
                           struct player_input *input);
void check_input_inactivity(struct player_input input, struct ghost *ghost);
void set_ghost_velocity(struct ghost *ghost, struct paddle paddle,
                        struct ball ball);
void update_paddle(struct paddle *paddle, double dt);
void set_ghost_speed(struct ghost *ghost);
void set_ghost_bias(struct ghost *ghost);
void set_ghost_idle_offset(struct ghost *ghost);
void update_ball(struct ball *ball, double dt, double t);
void check_ball_hit_wall(struct game *game);
void check_paddle_hit_ball(struct game *game);
void check_paddle_missed_ball(struct game *game);
void check_game_events(struct game *game, struct tonegen *tonegen);
void check_round_over(struct game *game);
void restart_round(struct game *game);
void check_round_restart_timeout(struct game *game);
void render_score(struct renderer_wrapper renderer_wrapper,
                  struct paddle paddle);
void render_net(struct renderer_wrapper renderer_wrapper);
void render_paddle(struct renderer_wrapper renderer_wrapper, struct game *game,
                   struct paddle paddle);
void render_ball(struct renderer_wrapper renderer_wrapper, struct ball ball);
void debug_render_ghost_ball(struct renderer_wrapper renderer_wrapper,
                             struct ball ball);

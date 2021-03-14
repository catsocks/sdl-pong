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
    bool active;
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

struct player_input {
    SDL_GameController *controller;
    SDL_TouchID touch_id;
    SDL_FingerID finger_id;
    int finger_y;
    bool finger_down;
    uint32_t last_input_timestamp;
};

struct game {
    SDL_Window *window;
    bool cheats_enabled;
    struct tonegen tonegen;
    struct paddle paddle_1;
    struct paddle paddle_2;
    float ghosts_sharpness;
    struct ghost ghost_1;
    struct ghost ghost_2;
    struct ball ball;
    struct ball ghost_ball;
    int max_score;
    struct player_input player_1_input;
    struct player_input player_2_input;
    bool first_player_input;
    uint32_t last_center_finger_down_timestamp;
    SDL_FingerID last_center_finger_down_finger_id;
    bool paused;
    bool debug_mode;
    double time;
    bool round_over;
    uint32_t round_restart_time;
    struct events events;
};

struct game make_game(SDL_Window *window, bool cheats_enabled);
void check_controller_added_event(struct game *game, SDL_Event event);
void check_controller_removed_event(struct game *game, SDL_Event event);
void check_finger_down_event(struct game *game, SDL_Event event);
void check_finger_up_event(struct game *game, SDL_Event event);
void check_finger_motion_event(struct game *game, SDL_Event event);
void check_keydown_event(struct game *game, SDL_Event event);
void toggle_fullscreen(struct game *game);
struct paddle make_paddle(int no);
struct ghost make_ghost(float ghosts_sharpness);
struct ball make_ball(int paddle_no, bool round_over, double t);
struct ball make_ghost_ball(struct ball ball, float ghosts_sharpness);
void check_paddle_controls(struct paddle *paddle, struct ghost *ghost,
                           struct player_input *input);
void check_player_activity(struct game *game, struct player_input input,
                           struct ghost *ghost);
void set_ghost_velocity(struct ghost *ghost, struct paddle paddle,
                        struct ball ball);
void update_paddle(struct paddle *paddle, double dt);
void set_ghost_speed(struct ghost *ghost, float ghosts_sharpness);
void set_ghost_bias(struct ghost *ghost);
void set_ghost_idle_offset(struct ghost *ghost);
void update_ball(struct ball *ball, double dt, double t);
void check_ball_hit_wall(struct game *game);
void check_paddle_hit_ball(struct game *game);
void check_paddle_missed_ball(struct game *game);
void check_game_events(struct game *game);
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

#include "player.hpp"
#include "assets.hpp"
#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 240
#define DINO_W 20
#define DINO_H 22
#define CACTUS_W 10
#define CACTUS_H 10
#define BACKGROUND_W 128
#define BACKGROUND_H 12
#define HORIZON_Y (SCREEN_H - BACKGROUND_H)
#define DINO_START_X 10
#define DINO_START_Y (HORIZON_Y - DINO_H)
#define CACTUS_SPAWN_X (SCREEN_W + 30)
#define CACTUS_BASE_Y (HORIZON_Y - (CACTUS_H))
#define DINO_RUNNING_MS_PER_FRAME 500

#define GRAVITY 100
#define JUMP_SPEED 30

#define START_x_speed 70
#define X_INCREASE 3

#ifndef TFT_DARKCYAN
#define TFT_DARKCYAN 0x03EF
#endif
#ifndef TFT_DARKGREEN
#define TFT_DARKGREEN 0x03E0
#endif

#define furi_get_tick() (millis() / 10)
#define furi_kernel_get_tick_frequency() 200
static void furi_hal_random_fill_buf(void *buffer, size_t len)
{
    uint8_t *buf = (uint8_t *)buffer;
    for (size_t i = 0; i < len; i++)
    {
        buf[i] = (uint8_t)random(256);
    }
}

// ---------------------------------------------------------------------------------
// GameState struct
// ---------------------------------------------------------------------------------
typedef struct
{
    int dino_frame_ms;
    uint32_t last_tick;
    // Dino info
    float y_position;
    float y_position_old;
    float y_speed;
    int y_acceleration;
    float x_speed;

    // Cactus info
    int cactus_position;
    int cactus_position_old;
    bool has_cactus;
    bool had_cactus;

    // Horizontal background lines - now using three positions
    int background_position;
    int background_position_old;

    bool lost;
    bool just_start;
    int score;
    const uint8_t *dino;
} GameState;

GameState *game_state = new GameState();

static void game_state_reinit(GameState *game_state)
{
    // Initialize game state
    game_state->dino_frame_ms = 0;
    game_state->y_acceleration = 0;
    game_state->y_speed = 0;
    game_state->y_position = DINO_START_Y;
    game_state->has_cactus = false;
    game_state->had_cactus = false;
    game_state->background_position = 0;
    game_state->lost = false;
    game_state->x_speed = START_x_speed;
    game_state->score = 0;
    game_state->just_start = true;
}

static void player_update(Entity *self, Game *game)
{
    // set old positions
    game_state->y_position_old = game_state->y_position;
    game_state->cactus_position_old = game_state->cactus_position;
    game_state->background_position_old = game_state->background_position;

    // track input (jump)
    if (game->input == BUTTON_UP)
    {
        // if we lost, restart
        if (game_state->lost)
        {
            game_state_reinit(game_state);
            game->input == -1;
            return;
        }
        // if on the ground, jump
        if (game_state->y_position == DINO_START_Y)
        {
            game_state->y_speed = JUMP_SPEED;
        }
    }

    uint32_t ticks_now = furi_get_tick();
    uint32_t ticks_elapsed = ticks_now - game_state->last_tick;
    game_state->last_tick = ticks_now;
    int delta_time_ms = ticks_elapsed * 1000 / furi_kernel_get_tick_frequency();

    // DINO animation frames
    game_state->dino_frame_ms += delta_time_ms;
    if (game_state->dino_frame_ms >= DINO_RUNNING_MS_PER_FRAME)
    {
        // toggle
        if (game_state->dino == dino_run_0_20x22)
            game_state->dino = dino_run_1_20x22;
        else
            game_state->dino = dino_run_0_20x22;

        game_state->dino_frame_ms = 0;
    }

    // DINO physics
    game_state->y_acceleration -= GRAVITY * (delta_time_ms / 1000.0f);
    game_state->y_speed += game_state->y_acceleration * (delta_time_ms / 1000.0f);
    game_state->y_position -= game_state->y_speed * (delta_time_ms / 1000.0f);

    // Touch ground
    if (game_state->y_position >= DINO_START_Y)
    {
        game_state->y_acceleration = 0;
        game_state->y_speed = 0;
        game_state->y_position = DINO_START_Y;
    }

    // CACTUS update
    if (game_state->has_cactus)
    {
        // move cactus left
        game_state->cactus_position -= (game_state->x_speed - 15) * (delta_time_ms / 1000.0f);
        if (game_state->cactus_position == 0)
        {
            // vanish, add to score
            game_state->has_cactus = false;
            game_state->score++;
            // speed up
            game_state->x_speed += X_INCREASE;
        }
    }
    else
    {
        // chance to spawn a new cactus
        uint8_t randomuint8[1];
        furi_hal_random_fill_buf(randomuint8, 1);
        // 1 out of 30 chance per frame
        if (randomuint8[0] % 30 == 0)
        {
            game_state->has_cactus = true;
            game_state->cactus_position = CACTUS_SPAWN_X;
        }
    }

    // Update background position for smooth scrolling
    int increase = (int)(game_state->x_speed * (delta_time_ms / 1000.0f));
    if (increase <= 0)
        increase = 1; // Ensure at least some movement

    // Move background position left
    game_state->background_position -= increase;

    // Wrap around when first background moves out of view
    // We use modulo to ensure the position always stays within one background width
    // This creates a seamless wrap-around effect
    if (game_state->background_position <= -BACKGROUND_W)
    {
        game_state->background_position = game_state->background_position % BACKGROUND_W;
    }

    if (game_state->has_cactus && ((game_state->y_position + DINO_H >= CACTUS_BASE_Y) &&
                                   ((DINO_START_X + DINO_W) >= game_state->cactus_position) &&
                                   (DINO_START_X <= (game_state->cactus_position + CACTUS_W))))
    {
        game_state->lost = true;
    }
}

static void player_render(Entity *self, Draw *draw, Game *game)
{
    if (!game_state->lost)
    {
        if (game_state->just_start)
        {
            game_state->just_start = false;
        }

        // Calculate the number of segments needed to cover the screen width
        int segments_needed = (SCREEN_W / BACKGROUND_W) + 2; // +2 to ensure we cover any partial segments

        // Draw enough horizon line segments to cover the entire screen width
        for (int i = 0; i < segments_needed; i++)
        {
            int segment_pos = game_state->background_position + (i * BACKGROUND_W);
            // Only draw segments that might be visible on screen
            if (segment_pos > -BACKGROUND_W && segment_pos < SCREEN_W)
            {
                draw->image(Vector(segment_pos, HORIZON_Y), horizon_line_0_128x12,
                            Vector(BACKGROUND_W, BACKGROUND_H), nullptr, false);
            }
        }

        // Dino
        draw->image(Vector(DINO_START_X, game_state->y_position), game_state->dino, Vector(DINO_W, DINO_H));

        // Cactus
        if (game_state->has_cactus)
        {
            draw->image(Vector(game_state->cactus_position,
                               CACTUS_BASE_Y),
                        cactus_10x10, Vector(CACTUS_W, CACTUS_H));
            game_state->had_cactus = true;
        }
        else if (game_state->had_cactus)
        {
            game_state->had_cactus = false;
        }

        char score_string[20];
        snprintf(score_string, sizeof(score_string), "Score: %d", game_state->score);
        draw->text(Vector(10, 10), score_string, game->fg_color);
    }
    else
    {
        draw->text(Vector(SCREEN_W / 2 - 30, SCREEN_H / 2),
                   "You lost :c", game->fg_color);
    }
}

// ---------------------------------------------------------------------------------
// Spawn the player entity and set up images
// ---------------------------------------------------------------------------------
void player_spawn(Level *level)
{
    // Create a blank entity
    Entity *player = new Entity("Player",
                                ENTITY_PLAYER,
                                Vector(-100, -100),
                                Vector(DINO_W, DINO_H),
                                NULL, NULL, NULL, NULL, NULL,
                                player_update,
                                player_render,
                                NULL,
                                true // is 8-bit?
    );
    level->entity_add(player);
    game_state_reinit(game_state);
}
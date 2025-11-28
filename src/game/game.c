#include <stdio.h>
#include <stdlib.h>
#include "geometry.h"
#include "platform_layer.h"

#define TARGET_RESOLUTION 1024
#define SPRITE_SIZE 64
#define DRAW_SIZE (vector2) { SPRITE_SIZE, SPRITE_SIZE }
#define SAMPLE_SIZE (vector2int) { SPRITE_SIZE, SPRITE_SIZE }
#define PLAYER_SPACESHIP_SAMPLE_POINT (vector2int){6 * SPRITE_SIZE, 0 }
#define PLAYER_SPACESHIP_SPEED (SPRITE_SIZE * 3.0f)
#define PLAYER_SPACESHIP_ANGULAR_SEED 2.0f
#define PLAYER_SPACESHIP_ACCELERATION (PLAYER_SPACESHIP_SPEED * 0.5f)

#define RESPAWN_INVINCIBILITY_DURATION 3.0f
#define RESPAWN_ANIMATION_DURATION 2.0f
#define EXPLOSION_ANIMATION_DURATION 0.5f
#define EXPLOSION_FRAME_COUNT 4
#define EXPLOSION_SAMPLE_POINT_START (vector2int){4 * SPRITE_SIZE, 3 * SPRITE_SIZE }

#define ASTEROID_LARGE_SAMPLE_POINT (vector2int){2 * SPRITE_SIZE, 3 * SPRITE_SIZE }
#define ASTEROID_MEDIUM_SAMPLE_POINT (vector2int){3 * SPRITE_SIZE, 3 * SPRITE_SIZE }
#define ASTEROID_SMALL_SAMPLE_POINT (vector2int){4 * SPRITE_SIZE, 3 * SPRITE_SIZE }

#define MAX_ASTEROIDS 128

typedef enum {
    ANIMATION_TYPE_NONE,
    ANIMATION_TYPE_RESPAWN,
    ANIMATION_TYPE_EXPLOSION,
} animation_type;

typedef struct {
    animation_type type;
    float time;
} animation;

typedef struct {
    vector2 position;
    vector2 direction;
    float speed;
    float angular_velocity;
    float rotation;
} transform;

typedef struct {
    transform transform;
    animation animation;
    float invincibility_time_remaining;
    bool is_destroyed;
} spaceship;

typedef enum {
    ASTEROID_SIZE_SMALL,
    ASTEROID_SIZE_MEDIUM,
    ASTEROID_SIZE_LARGE,
} asteroid_size;

typedef enum {
    ASTEROID_CONTENT_NONE,
    ASTEROID_CONTENT_ORE,
} asteroid_content;

typedef struct {
    transform transform;
    asteroid_size size;
    asteroid_content content;
} asteroid;

DECLARE_CAPPED_ARRAY(asteroids, asteroid, MAX_ASTEROIDS)
IMPLEMENT_CAPPED_ARRAY(asteroids, asteroid, MAX_ASTEROIDS)

typedef struct {
    spaceship player_spaceship;
    asteroids asteroids;
} game_state;

static void apply_velocity(transform* transform, float delta_time) {
    ASSERT(transform != NULL, return, "Transform cannot be NULL");
    transform->position = vector2_add(transform->position, vector2_scale(transform->direction, transform->speed * delta_time));
}

typedef enum {
    ROTATION_NORMAL,
    ROTATION_CHANGES_MOVEMENT_DIRECTION,
} rotation_mode;

static void apply_angular_velocity(transform* transform, float delta_time) {
    ASSERT(transform != NULL, return, "Transform cannot be NULL");
    if (transform->angular_velocity == 0.0f) return;
    transform->rotation += transform->angular_velocity * delta_time;
}

static void control_player_spaceship(spaceship* player, input* input, float delta_time) {
    ASSERT(player != NULL, return, "Player spaceship cannot be NULL");
    ASSERT(input != NULL, return, "Input state cannot be NULL");

    if (player->is_destroyed) {
        return;
    }

    const float direction_offset_radians = -M_PI / 2.0f;
    player->transform.angular_velocity = 0.0f;

    if (is_key_held_down(input, KEY_A)) {
        player->transform.angular_velocity += PLAYER_SPACESHIP_ANGULAR_SEED;
        player->transform.direction = vector2_from_angle(-player->transform.rotation * M_PI + direction_offset_radians);
    }

    if (is_key_held_down(input, KEY_D)) {
        player->transform.angular_velocity -= PLAYER_SPACESHIP_ANGULAR_SEED;
        player->transform.direction = vector2_from_angle(-player->transform.rotation * M_PI + direction_offset_radians);
    }

    player->transform.rotation += player->transform.angular_velocity * delta_time;

    if (is_key_held_down(input, KEY_W)) {
        player->transform.speed += PLAYER_SPACESHIP_ACCELERATION * delta_time * 0.5f;
        if (player->transform.speed > PLAYER_SPACESHIP_SPEED) {
            player->transform.speed = PLAYER_SPACESHIP_SPEED;
        }
        player->transform.position = vector2_add(player->transform.position,
            vector2_scale(player->transform.direction, player->transform.speed * delta_time));
        player->transform.speed += PLAYER_SPACESHIP_ACCELERATION * delta_time * 0.5f;
        if (player->transform.speed > PLAYER_SPACESHIP_SPEED) {
            player->transform.speed = PLAYER_SPACESHIP_SPEED;
        }
    }
    else if (is_key_held_down(input, KEY_S)) {
        player->transform.speed -= PLAYER_SPACESHIP_ACCELERATION * delta_time * 0.5f;
        if (player->transform.speed < -PLAYER_SPACESHIP_SPEED * 0.5f) {
            player->transform.speed = -PLAYER_SPACESHIP_SPEED * 0.5f;
        }
        player->transform.position = vector2_add(player->transform.position,
            vector2_scale(player->transform.direction, player->transform.speed * delta_time));
        player->transform.speed -= PLAYER_SPACESHIP_ACCELERATION * delta_time * 0.5f;
        if (player->transform.speed < -PLAYER_SPACESHIP_SPEED * 0.5f) {
            player->transform.speed = -PLAYER_SPACESHIP_SPEED * 0.5f;
        }
    }
    else {
        player->transform.position = vector2_add(player->transform.position,
            vector2_scale(player->transform.direction, player->transform.speed * delta_time));
    }

    if (player->transform.position.x < 0.0f) {
        player->transform.position.x += TARGET_RESOLUTION;
    }
    else if (player->transform.position.x > TARGET_RESOLUTION) {
        player->transform.position.x -= TARGET_RESOLUTION;
    }

    if (player->transform.position.y < 0.0f) {
        player->transform.position.y += TARGET_RESOLUTION;
    }
    else if (player->transform.position.y > TARGET_RESOLUTION) {
        player->transform.position.y -= TARGET_RESOLUTION;
    }
}

static void update_simulation(game_state* state, float delta_time) {
    ASSERT(state != NULL, return, "State cannot be NULL");

    for (uint32_t i = 0; i < state->asteroids.count; ++i) {
        asteroid* asteroid = &state->asteroids.elements[i];
        apply_angular_velocity(&asteroid->transform, delta_time);
        apply_velocity(&asteroid->transform, delta_time);

        if (asteroid->transform.position.x < 0.0f) {
            asteroid->transform.position.x += TARGET_RESOLUTION;
        }
        else if (asteroid->transform.position.x > TARGET_RESOLUTION) {
            asteroid->transform.position.x -= TARGET_RESOLUTION;
        }

        if (asteroid->transform.position.y < 0.0f) {
            asteroid->transform.position.y += TARGET_RESOLUTION;
        }
        else if (asteroid->transform.position.y > TARGET_RESOLUTION) {
            asteroid->transform.position.y -= TARGET_RESOLUTION;
        }
    }

    if (!state->player_spaceship.is_destroyed && state->player_spaceship.invincibility_time_remaining <= 0.0f) {
        for (uint32_t i = 0; i < state->asteroids.count; ++i) {
            asteroid* ast = &state->asteroids.elements[i];

            // check collision with player spaceship
            vector2 to_player = vector2_sub(state->player_spaceship.transform.position, ast->transform.position);
            float distance_sq = to_player.x * to_player.x + to_player.y * to_player.y;
            float collision_distance = SPRITE_SIZE; // approximate both as circles with radius SPRITE_SIZE/2
            if (distance_sq < collision_distance * collision_distance) {
                // Collision detected
                state->player_spaceship.animation.type = ANIMATION_TYPE_EXPLOSION;
                state->player_spaceship.animation.time = 0.0f;
                state->player_spaceship.transform.speed = 0.0f;
                state->player_spaceship.transform.angular_velocity = 0.0f;
                state->player_spaceship.is_destroyed = true;
            }
        }
    }

    if (state->player_spaceship.invincibility_time_remaining > 0.0f) {
        state->player_spaceship.invincibility_time_remaining -= delta_time;
        if (state->player_spaceship.invincibility_time_remaining < 0.0f) {
            state->player_spaceship.invincibility_time_remaining = 0.0f;
        }
    }

    if (state->player_spaceship.is_destroyed && state->player_spaceship.animation.type == ANIMATION_TYPE_NONE) {
        // Respawn the player spaceship after explosion animation
        memset(&state->player_spaceship, 0, sizeof(spaceship));
        state->player_spaceship.is_destroyed = false;
        state->player_spaceship.transform.position = (vector2){ TARGET_RESOLUTION / 2.0f, TARGET_RESOLUTION / 2.0f };
        state->player_spaceship.transform.speed = 0.0f;
        state->player_spaceship.transform.direction = (vector2){ 0.0f, -1.0f };
        state->player_spaceship.transform.rotation = 0.0f;
        state->player_spaceship.animation.type = ANIMATION_TYPE_RESPAWN;
        state->player_spaceship.animation.time = 0.0f;
        state->player_spaceship.invincibility_time_remaining = RESPAWN_INVINCIBILITY_DURATION;
    }
}

static void draw_player_spaceship(spaceship* player, graphics* graphics, float delta_time) {
    ASSERT(player != NULL, return, "Player spaceship cannot be NULL");
    ASSERT(graphics != NULL, return, "Graphics cannot be NULL");

    if (player->animation.type == ANIMATION_TYPE_NONE && !player->is_destroyed) {
        draw_sprite(graphics, player->transform.position, DRAW_SIZE,
            PLAYER_SPACESHIP_SAMPLE_POINT,
            SAMPLE_SIZE,
            player->transform.rotation * M_PI);
        return;
    }

    player->animation.time += delta_time;

    if (player->animation.type == ANIMATION_TYPE_RESPAWN) {
        float scale_factor = player->animation.time / RESPAWN_ANIMATION_DURATION;
        if (scale_factor > 1.0f) {
            scale_factor = 1.0f;
            player->animation.type = ANIMATION_TYPE_NONE;
            player->animation.time = 0.0f;
            return;
        }
        vector2 draw_size = vector2_scale(DRAW_SIZE, scale_factor);
        draw_sprite(graphics, player->transform.position, draw_size,
            PLAYER_SPACESHIP_SAMPLE_POINT,
            SAMPLE_SIZE,
            player->transform.rotation * M_PI);
        return;
    }

    if (player->animation.type == ANIMATION_TYPE_EXPLOSION) {
        uint32_t current_frame = (uint32_t)((player->animation.time / EXPLOSION_ANIMATION_DURATION) * (float)EXPLOSION_FRAME_COUNT);
        if (current_frame >= EXPLOSION_FRAME_COUNT) {
            // Animation finished
            player->animation.type = ANIMATION_TYPE_NONE;
            player->animation.time = 0.0f;
            return;
        }

        vector2int sample_point = (vector2int){
            EXPLOSION_SAMPLE_POINT_START.x + (current_frame * SPRITE_SIZE),
            EXPLOSION_SAMPLE_POINT_START.y
        };

        draw_sprite(graphics, player->transform.position, DRAW_SIZE,
            sample_point,
            SAMPLE_SIZE,
            player->transform.rotation * M_PI);

        return;
    }

    if (player->is_destroyed) {
        // If destroyed but no animation, don't draw anything
        return;
    }

    DEBUG_ASSERT(player->animation.type == ANIMATION_TYPE_NONE, , "Unknown animation type for player spaceship.");
    // This code should never execute, but just in case, draw the spaceship normally
    draw_sprite(graphics, player->transform.position, DRAW_SIZE,
        PLAYER_SPACESHIP_SAMPLE_POINT,
        SAMPLE_SIZE,
        player->transform.rotation * M_PI);
    return;
}

static void draw_simulation(game_state* state, graphics* graphics, float delta_time) {
    ASSERT(state != NULL, return, "State cannot be NULL");
    ASSERT(graphics != NULL, return, "Graphics cannot be NULL");
    color background_color = color_from_uint32(0x222323);
    draw_background_color(graphics, background_color.r, background_color.g, background_color.b, background_color.a);

    // Draw asteroids
    for (uint32_t i = 0; i < state->asteroids.count; ++i) {
        asteroid* ast = &state->asteroids.elements[i];
        draw_sprite(graphics, ast->transform.position, DRAW_SIZE,
            (ast->size == ASTEROID_SIZE_LARGE) ? ASTEROID_LARGE_SAMPLE_POINT :
            (ast->size == ASTEROID_SIZE_MEDIUM) ? ASTEROID_MEDIUM_SAMPLE_POINT :
            ASTEROID_SMALL_SAMPLE_POINT,
            SAMPLE_SIZE,
            ast->transform.rotation * M_PI);
    }

    // Draw player spaceship
    draw_player_spaceship(&state->player_spaceship, graphics, delta_time);
}

DLL_EXPORT result init(init_in_params* in, init_out_params* out) {
    game_state* state = (game_state*)bump_allocate(&in->memory_allocators->perm, alignof(game_state), sizeof(game_state));
    if (state == NULL) {
        BUG("Failed to allocate memory for game state.");
        return RESULT_FAILURE;
    }

    {
        spaceship* player = &state->player_spaceship;
        memset(player, 0, sizeof(spaceship));
        player->transform.position = (vector2){ TARGET_RESOLUTION / 2.0f, TARGET_RESOLUTION / 2.0f };
    }

    {
        // Initialize some asteroids for demonstration
        for (uint32_t i = 0; i < 10; ++i) {
            asteroid* ast = &state->asteroids.elements[state->asteroids.count++];
            ast->transform.position = (vector2){ (float)(rand() % (TARGET_RESOLUTION)),
                                                 (float)(rand() % (TARGET_RESOLUTION)) };
            float angle = (float)(rand() % 360) * (M_PI / 180.0f);
            float speed = (float)(50 + rand() % 100);
            ast->transform.direction = vector2_from_angle(angle);
            ast->transform.speed = speed;
            ast->transform.rotation = 0.0f;
            ast->transform.angular_velocity = ((float)(rand() % 100) / 100.0f) * 1.0f;
            ast->size = ASTEROID_SIZE_LARGE;
            ast->content = ASTEROID_CONTENT_NONE;
        }
    }

    out->game_state = (void*)state;
    out->virtual_resolution = (vector2int){ 1024, 1024 };
    return RESULT_SUCCESS;
}

DLL_EXPORT result start(start_params* in) {
    ASSERT(in->game_state, return RESULT_FAILURE, "Game state is NULL in start.");
    return RESULT_SUCCESS;
}

DLL_EXPORT result update(update_params* in) {
    ASSERT(in->game_state, return RESULT_FAILURE, "Game state is NULL in update.");
    game_state* state = (game_state*)in->game_state;
    control_player_spaceship(&state->player_spaceship, in->input, in->delta_time);
    update_simulation(state, in->delta_time);
    return RESULT_SUCCESS;
}

DLL_EXPORT void draw(draw_params* in) {
    ASSERT(in->game_state, return, "Game state is NULL in draw.");
    ASSERT(in->graphics, return, "Graphics context is NULL in draw.");
    game_state* state = (game_state*)in->game_state;
    draw_simulation(state, in->graphics, in->delta_time);
}

DLL_EXPORT void cleanup(cleanup_params* in) {
    (void)in;
}

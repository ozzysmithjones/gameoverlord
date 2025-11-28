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

#define ASTEROID_LARGE_SAMPLE_POINT (vector2int){2 * SPRITE_SIZE, 3 * SPRITE_SIZE }
#define ASTEROID_MEDIUM_SAMPLE_POINT (vector2int){3 * SPRITE_SIZE, 3 * SPRITE_SIZE }
#define ASTEROID_SMALL_SAMPLE_POINT (vector2int){4 * SPRITE_SIZE, 3 * SPRITE_SIZE }

#define MAX_ASTEROIDS 128

typedef struct {
    vector2 position;
    vector2 velocity;
    float angular_velocity;
    float rotation;
} transform;

typedef struct {
    transform transform;
    uint32_t hit_points;
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
    camera_2d camera;
    spaceship player_spaceship;
    asteroids asteroids;
} game_state;

static void apply_velocity(transform* transform, float delta_time) {
    ASSERT(transform != NULL, return, "Transform cannot be NULL");
    transform->position = vector2_add(transform->position, vector2_scale(transform->velocity, delta_time));
}

typedef enum {
    ROTATION_NORMAL,
    ROTATION_CHANGES_MOVEMENT_DIRECTION,
} rotation_mode;

static void apply_angular_velocity(transform* transform, float delta_time, rotation_mode mode) {
    ASSERT(transform != NULL, return, "Transform cannot be NULL");
    if (transform->angular_velocity == 0.0f) return;
    transform->rotation += transform->angular_velocity * delta_time;
    if (mode == ROTATION_CHANGES_MOVEMENT_DIRECTION) {
        transform->velocity = vector2_scale(vector2_from_angle(transform->rotation * M_PI), PLAYER_SPACESHIP_SPEED);
    }
}

static void handle_user_input(game_state* state, input* input) {
    ASSERT(state != NULL, return, "Game state cannot be NULL");
    ASSERT(input != NULL, return, "Input state cannot be NULL");
    spaceship* player = &state->player_spaceship;
    player->transform.angular_velocity = 0.0f;
    if (is_key_held_down(input, KEY_A)) {
        player->transform.angular_velocity += PLAYER_SPACESHIP_ANGULAR_SEED;
    }
    if (is_key_held_down(input, KEY_D)) {
        player->transform.angular_velocity -= PLAYER_SPACESHIP_ANGULAR_SEED;
    }
}

static void update_simulation(game_state* state, float delta_time) {
    ASSERT(state != NULL, return, "State cannot be NULL");
    apply_angular_velocity(&state->player_spaceship.transform, delta_time, ROTATION_CHANGES_MOVEMENT_DIRECTION);
    apply_velocity(&state->player_spaceship.transform, delta_time);

    for (uint32_t i = 0; i < state->asteroids.count; ++i) {
        asteroid* asteroid = &state->asteroids.elements[i];
        apply_angular_velocity(&asteroid->transform, delta_time, ROTATION_NORMAL);
        apply_velocity(&asteroid->transform, delta_time);
    }
}

static void draw_simulation(game_state* state, graphics* graphics) {
    ASSERT(state != NULL, return, "State cannot be NULL");
    ASSERT(graphics != NULL, return, "Graphics cannot be NULL");
    color background_color = color_from_uint32(0x222323);
    draw_background_color(graphics, background_color.r, background_color.g, background_color.b, background_color.a);

    // Draw asteroids
    for (uint32_t i = 0; i < state->asteroids.count; ++i) {
        asteroid* ast = &state->asteroids.elements[i];
        draw_projected_sprite(graphics, &state->camera, ast->transform.position, DRAW_SIZE,
            (ast->size == ASTEROID_SIZE_LARGE) ? ASTEROID_LARGE_SAMPLE_POINT :
            (ast->size == ASTEROID_SIZE_MEDIUM) ? ASTEROID_MEDIUM_SAMPLE_POINT :
            ASTEROID_SMALL_SAMPLE_POINT,
            SAMPLE_SIZE,
            ast->transform.rotation);
    }

    // Draw player spaceship
    draw_projected_sprite(graphics, &state->camera, state->player_spaceship.transform.position, DRAW_SIZE,
        PLAYER_SPACESHIP_SAMPLE_POINT,
        SAMPLE_SIZE,
        state->player_spaceship.transform.rotation);
}

DLL_EXPORT result init(init_in_params* in, init_out_params* out) {
    game_state* state = (game_state*)bump_allocate(&in->memory_allocators->perm, alignof(game_state), sizeof(game_state));
    if (state == NULL) {
        BUG("Failed to allocate memory for game state.");
        return RESULT_FAILURE;
    }

    state->camera.offset = (vector2){ TARGET_RESOLUTION / 2.0f, TARGET_RESOLUTION / 2.0f };
    state->camera.position = (vector2){ 0, 0 };
    state->camera.zoom = 1.0f;


    {
        spaceship* player = &state->player_spaceship;
        memset(player, 0, sizeof(spaceship));
    }

    /*
    {
        // Initialize some asteroids for demonstration
        for (uint32_t i = 0; i < 10; ++i) {
            asteroid* ast = &state->asteroids.elements[state->asteroids.count++];
            ast->transform.position = (vector2){ (float)(rand() % (TARGET_RESOLUTION)) - TARGET_RESOLUTION / 2.0f,
                                                 (float)(rand() % (TARGET_RESOLUTION)) - TARGET_RESOLUTION / 2.0f };
            float angle = (float)(rand() % 360) * (3.14159f / 180.0f);
            float speed = (float)(50 + rand() % 100);
            ast->transform.velocity = vector2_scale(vector2_from_angle(angle), speed);
            ast->transform.rotation = 0.0f;
            ast->transform.angular_velocity = ((float)(rand() % 100) / 100.0f) * 1.0f;
            ast->size = ASTEROID_SIZE_LARGE;
            ast->content = ASTEROID_CONTENT_NONE;
        }
    }
        */

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
    handle_user_input(state, in->input);
    update_simulation(state, in->delta_time);
    state->camera.position = state->player_spaceship.transform.position;
    return RESULT_SUCCESS;
}

DLL_EXPORT void draw(draw_params* in) {
    ASSERT(in->game_state, return, "Game state is NULL in draw.");
    ASSERT(in->graphics, return, "Graphics context is NULL in draw.");
    game_state* state = (game_state*)in->game_state;
    draw_simulation(state, in->graphics);
}

DLL_EXPORT void cleanup(cleanup_params* in) {
    (void)in;
}

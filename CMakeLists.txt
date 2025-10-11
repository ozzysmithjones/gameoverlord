cmake_minimum_required(VERSION 3.10.0)
project(gameoverlord VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Glob source files
# file(GLOB_RECURSE SOURCES
# "src/*.c"
# "src/*.h"
# )

set(ENGINE_SOURCES
    src/fundamental.h
    src/platform_layer.h
    src/windows_platform_layer.c
)

set(GAME_SOURCES
    src/fundamental.h
    src/game.c
    src/game.h
)

# Update executable with source files
add_executable(engine WIN32 ${ENGINE_SOURCES})
add_library(game SHARED ${GAME_SOURCES})
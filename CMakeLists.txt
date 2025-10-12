cmake_minimum_required(VERSION 3.10.0)
project(gameoverlord VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Glob source files
# file(GLOB_RECURSE SOURCES
# "src/*.c"
# "src/*.h"
# )

SET(COMMON_SOURCES
    src/fundamental.h
    src/geometry.h 
    src/platform_layer.h 
    src/windows_platform_layer.c 
    src/stb_image.h
   )
SET(ENGINE_SOURCES
    ${COMMON_SOURCES}
   )
SET(GAME_SOURCES
    ${COMMON_SOURCES}
    src/game.c
    src/game.h
   )

# Update executable with source files
if (WIN32)
add_executable(engine WIN32 ${ENGINE_SOURCES})
target_compile_definitions(engine PRIVATE APP_BACKEND HOT_RELOAD_HOST)
target_link_libraries(engine d3d11 dxgi d3dcompiler)
else()
error("Unsupported platform")
endif()
add_library(game SHARED ${GAME_SOURCES})
cmake_minimum_required(VERSION 3.10.0)
project(gameoverlord VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED OFF)

SET(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/engine)
file(GLOB_RECURSE ENGINE_SOURCES 
    ${ENGINE_DIR}/*.c
    ${ENGINE_DIR}/*.h
   )

SET(GAME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/game)
file(GLOB_RECURSE GAME_SOURCES 
    ${GAME_DIR}/*.c
    ${GAME_DIR}/*.h
   )

SET(HOT_RELOAD OFF)

if(HOT_RELOAD)
    add_executable(engine WIN32 ${ENGINE_SOURCES})
  
    # Set the game engine to be the main game-loop and the hot-reload host
    target_compile_definitions(engine PRIVATE GAME_LOOP HOT_RELOAD_HOST)
    target_link_libraries(engine d3d11 dxgi d3dcompiler xaudio2)
    
    # Set the game to be a shared library that can be hot-reloaded (game-loop is disabled while it is handled by the game engine)
    add_library(game SHARED ${ENGINE_SOURCES} ${GAME_SOURCES})
    target_include_directories(game PRIVATE ${ENGINE_DIR})
    
    # Disable game debug information so that it doesn't interfere with hot-reloading
    set_target_properties(game PROPERTIES
        COMPILE_PDB_NAME ""
        COMPILE_PDB_OUTPUT_DIRECTORY ""
        PDB_NAME ""
        PDB_OUTPUT_DIRECTORY "")
            
    target_link_options(game PRIVATE "/DEBUG:NONE")
else()
    add_executable(game WIN32 ${ENGINE_SOURCES} ${GAME_SOURCES})
    target_compile_definitions(game PRIVATE GAME_LOOP)
    target_include_directories(game PRIVATE ${ENGINE_DIR})
    target_link_libraries(game d3d11 dxgi d3dcompiler xaudio2)
endif()
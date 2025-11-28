cmake_minimum_required(VERSION 3.10.0)
project(gameoverlord VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED OFF)

# Glob source files
# file(GLOB_RECURSE SOURCES
# "src/*.c"
# "src/*.h"
# )

SET(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/engine)
file(GLOB_RECURSE ENGINE_SOURCES 
    ${ENGINE_DIR}/*.c
    ${ENGINE_DIR}/*.h
   )


SET(GAME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/game)
SET(GAME_SOURCES
    ${ENGINE_SOURCES}
    ${GAME_DIR}/game.c
    ${GAME_DIR}/game.h
   )

SET(HOT_RELOAD OFF)

if(HOT_RELOAD)
    add_executable(engine WIN32 ${ENGINE_SOURCES})
    target_compile_definitions(engine PRIVATE GAME_LOOP HOT_RELOAD_HOST)
    target_link_libraries(engine d3d11 dxgi d3dcompiler)
    
    add_library(game SHARED ${GAME_SOURCES})
    target_include_directories(game PRIVATE ${ENGINE_DIR})
    
    # Disable PDB generation so that it doesn't interfere with hot-reloading
    set_target_properties(game PROPERTIES
        COMPILE_PDB_NAME ""
        COMPILE_PDB_OUTPUT_DIRECTORY ""
        PDB_NAME ""
        PDB_OUTPUT_DIRECTORY "")
            
    target_link_options(game PRIVATE "/DEBUG:NONE")

    # copy assets folder to output directory
    add_custom_command(TARGET game POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:engine>/assets
    )

else()
    add_executable(app WIN32 ${GAME_SOURCES})
    target_compile_definitions(app PRIVATE GAME_LOOP)
    target_include_directories(app PRIVATE ${ENGINE_DIR})
    target_link_libraries(app d3d11 dxgi d3dcompiler)

    add_custom_command(TARGET app POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:app>/assets
    )

endif()
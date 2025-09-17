cmake_minimum_required(VERSION 3.10.0)
project(gameoverlord VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Disable exceptions
if(MSVC)
    add_compile_options(/EHs-c-)
    add_definitions(-D_HAS_EXCEPTIONS=0)
else()
    add_compile_options(-fno-exceptions)
endif()

# Glob source files
file(GLOB_RECURSE SOURCES
"src/*.c"
"src/*.h"
)

# Update executable with source files
add_executable(gameoverlord ${SOURCES})


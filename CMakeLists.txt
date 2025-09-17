cmake_minimum_required(VERSION 3.10.0)
project(gameoverlord VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Glob source files
file(GLOB_RECURSE SOURCES
"src/*.c"
"src/*.h"
)

# Update executable with source files
add_executable(gameoverlord ${SOURCES})


# Find Vulkan package
find_package(Vulkan REQUIRED)

# Include Vulkan directories
target_include_directories(gameoverlord PRIVATE ${Vulkan_INCLUDE_DIRS})

# Link against Vulkan
target_link_directories(gameoverlord PRIVATE ${Vulkan_LIBRARY_DIRS})
target_link_libraries(gameoverlord PRIVATE Vulkan::Vulkan)

# If you're on Windows and need to link with the Vulkan SDK directly
if(WIN32)
    # You might need to set Vulkan SDK path if CMake can't find it automatically
    # set(VULKAN_SDK_PATH "C:/VulkanSDK/x.x.x.x")
    # target_include_directories(gameoverlord PRIVATE "${VULKAN_SDK_PATH}/Include")
    # target_link_directories(gameoverlord PRIVATE "${VULKAN_SDK_PATH}/Lib")
    # target_link_libraries(gameoverlord PRIVATE vulkan-1)
endif()
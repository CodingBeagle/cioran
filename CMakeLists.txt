# cmake_minimum_required should be the first command in your CMakeLists file.
# This sets the minimum required version of cmake for this project.
cmake_minimum_required(VERSION 3.29)

# Set the C++ language standard to be 20, and to be required
# If the compiler on the platform does not support 20, then the CMAKE build will fail.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set environment variable for all targets
set(ENV{DISABLE_VK_LAYER_VALVE_steam_overlay_1} "1")

# Disable C++ compiler extensions. This is best practice.
# Many compilers implement their own changes to the language, to enhance compability with other versions of the language
# or for historical reasons. These compiler-specific behaviours are called "compiler extensions".
# Writing programs that makes use of a compiler extension allows you to write programs that are incompatible with the C++ standard.
# You can have code that is actually over-permissive, and won't work on other compilers, or may not run correctly.
# Compiler extensions are often enabled by default.
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the name for the project.
# Also specifies that the supported language is C++
project(cioran LANGUAGES CXX)

# Add executable target
# A target corresponds to an executable or a library.
add_executable(${PROJECT_NAME} src/main.cpp)

# Add include directories from "headers" directory
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/headers)

# Add vulkan library
# This uses the Module mode of find_pckage to find the Vulkan package on the system.
# In this case, CMake provides a "Find Module" for Vulkan, which is "FindVulkan.cmake.
# This module is used to find the Vulkan package on the system, using a set of usually known paths for Vulkan.
# If successful, it will set the Vulkan_INCLUDE_DIRS and Vulkan_LIBRARIES variables, which can be used to include the Vulkan headers and link against the Vulkan libraries.
find_package(Vulkan REQUIRED)

# Add include directories for Vulkan
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})

# Link against Vulkan libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARIES})

# Dynamically link to SDL3
# Set the path to the DLL directory
set(SDL_DLL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendors/sdl)

# Add the DLL directory to the runtime search path
target_link_directories(${PROJECT_NAME} PRIVATE ${SDL_DLL_DIR})
# Assuming SDL3-x64-debug.lib is the import library corresponding to SDL3-x64-debug.dll

# Link against the SDL3 import library
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3-x64-debug)

# Add a post-build command to copy the SDL3 DLL to the output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL_DLL_DIR}/SDL3-x64-debug.dll"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)
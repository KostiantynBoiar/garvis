cmake_minimum_required(VERSION 3.20)
project(garvis VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GARVIS_GPU_CUDA   "Enable CUDA acceleration for llama.cpp"   OFF)
option(GARVIS_GPU_VULKAN "Enable Vulkan acceleration for llama.cpp" OFF)

include(FetchContent)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    llama_cpp
    GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)

if(GARVIS_GPU_CUDA)
    set(GGML_CUDA ON CACHE BOOL "" FORCE)
endif()
if(GARVIS_GPU_VULKAN)
    set(GGML_VULKAN ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    whisper_cpp
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib llama_cpp whisper_cpp)

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${CMAKE_SOURCE_DIR}/third_party/miniaudio)

file(GLOB_RECURSE GARVIS_SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

add_executable(garvis ${GARVIS_SOURCES})

target_include_directories(garvis PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(garvis PRIVATE
    raylib
    llama
    whisper
    miniaudio
    winmm
    ole32
)

if(MSVC)
    target_compile_options(garvis PRIVATE /W4)
else()
    target_compile_options(garvis PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_custom_command(TARGET garvis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:garvis>/assets
    COMMENT "Copying assets to build directory"
)

install(TARGETS garvis RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION bin/assets)

message(STATUS "  CUDA   : ${GARVIS_GPU_CUDA}")
message(STATUS "  Vulkan : ${GARVIS_GPU_VULKAN}")

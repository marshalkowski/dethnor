cmake_minimum_required(VERSION 3.20)

project(DungeonsOfDethnor
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(vendor/Bengine)

add_executable(dungeons-of-dethnor
    src/main.cpp
    src/Room.cpp
    src/Player.cpp
    src/RoomCamera.cpp
)

target_link_libraries(dungeons-of-dethnor
    PRIVATE
        bengine
)

target_include_directories(dungeons-of-dethnor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Bengine E1 asset workflow (see vendor/Bengine's README.md and
# examples/14_camera_basic): copy our own assets/ next to the built
# executable so Engine::SetAssetRoot("assets") (called in main.cpp) resolves
# to something, regardless of working directory or build config.
add_custom_command(TARGET dungeons-of-dethnor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:dungeons-of-dethnor>/assets"
)
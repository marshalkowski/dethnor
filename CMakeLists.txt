cmake_minimum_required(VERSION 3.20)

project(DungeonsOfDethnor
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(vendor/Bengine)

# nlohmann/json 3.12.0, vendored as a single header (vendor/nlohmann_json).
# Header-only, so an INTERFACE target that just carries the include path.
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/nlohmann_json
)

add_executable(dungeons-of-dethnor
    src/main.cpp
    src/Character.cpp
    src/CombatSystem.cpp
    src/Content.cpp
    src/SkeletonAI.cpp
    src/PlayerControl.cpp
    src/LevelDefinition.cpp
    src/LevelRuntime.cpp
    src/LevelCamera.cpp
    src/Hud.cpp
    src/TitleScreen.cpp
)

target_link_libraries(dungeons-of-dethnor
    PRIVATE
        bengine
        nlohmann_json
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

# Headless data check: loads every JSON file under assets/data through the
# same LoadContent the game uses and verifies it against the asset files on
# disk. No window or GPU needed -- run with `ctest -C Debug` from the build
# directory.
enable_testing()
add_executable(dethnor-content-check
    tests/content_check.cpp
    src/Content.cpp
    src/LevelDefinition.cpp
)
target_link_libraries(dethnor-content-check PRIVATE bengine nlohmann_json)
target_include_directories(dethnor-content-check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_compile_definitions(dethnor-content-check PRIVATE
    DETHNOR_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
add_test(NAME content-check COMMAND dethnor-content-check)

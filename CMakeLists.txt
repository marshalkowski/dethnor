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
    src/EnemyAI.cpp
    src/PlayerControl.cpp
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
)
target_link_libraries(dethnor-content-check PRIVATE bengine nlohmann_json)
target_include_directories(dethnor-content-check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_compile_definitions(dethnor-content-check PRIVATE
    DETHNOR_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
add_test(NAME content-check COMMAND dethnor-content-check)

# Headless gameplay simulation: real Characters, AI and combat driven at a
# fixed step against the real loaded data (no window, no textures).
add_executable(dethnor-ai-sim
    tests/ai_sim.cpp
    src/Content.cpp
    src/Character.cpp
    src/CombatSystem.cpp
    src/EnemyAI.cpp
)
target_link_libraries(dethnor-ai-sim PRIVATE bengine nlohmann_json)
target_include_directories(dethnor-ai-sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_compile_definitions(dethnor-ai-sim PRIVATE
    DETHNOR_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
add_test(NAME ai-sim COMMAND dethnor-ai-sim)

# Headless level flow: door/spawn/session chaining across world1's levels,
# plus a scripted bot playing each level through to its exit.
add_executable(dethnor-level-flow
    tests/level_flow.cpp
    src/Content.cpp
    src/Character.cpp
    src/CombatSystem.cpp
    src/EnemyAI.cpp
    src/LevelRuntime.cpp
)
target_link_libraries(dethnor-level-flow PRIVATE bengine nlohmann_json)
target_include_directories(dethnor-level-flow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_compile_definitions(dethnor-level-flow PRIVATE
    DETHNOR_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
add_test(NAME level-flow COMMAND dethnor-level-flow)

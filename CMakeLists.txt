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
)

target_link_libraries(dungeons-of-dethnor
    PRIVATE
        bengine
)

target_include_directories(dungeons-of-dethnor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Bengine has no asset-loading/search-path concept yet, so (matching every
# Bengine example) bake in the absolute path to our own asset directory at
# compile time.
target_compile_definitions(dungeons-of-dethnor
    PRIVATE
        DETHNOR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
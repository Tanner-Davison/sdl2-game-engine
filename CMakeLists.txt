cmake_minimum_required(VERSION 3.15)
project(sdl-sandbox
        VERSION 1.0
        DESCRIPTION "SDL3 Game Engine Sandbox"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release if no build type is specified.
# Release enables -O3 + NDEBUG which can be 3-10x faster than unoptimized debug builds.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(SDL3_STATIC OFF)
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)

set(SOURCES
    src/main.cpp
    src/TitleScene.cpp
    src/GameScene.cpp
    src/LevelTwo.cpp
    src/LevelEditorScene.cpp
    src/EditorSurfaceCache.cpp
    src/Window.cpp
    src/Rectangle.cpp
    src/UI.cpp
    src/Button.cpp
    src/Image.cpp
    src/Text.cpp
    src/SpriteSheet.cpp
    src/PlayerCreatorScene.cpp
    src/TileAnimCreatorScene.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include          # root: Components, GameConfig, GameEvents, ...
    ${CMAKE_CURRENT_SOURCE_DIR}/include/engine   # engine/Scene.hpp, engine/SceneManager.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/game     # game/GameConfig.hpp, game/GameEvents.hpp
    # systems/ is reachable as <systems/Foo.hpp> via the include/ root above
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    EnTT::EnTT
    nlohmann_json::nlohmann_json
    PNG::PNG
    ZLIB::ZLIB
)

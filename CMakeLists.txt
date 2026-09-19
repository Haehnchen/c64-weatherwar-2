cmake_minimum_required(VERSION 3.20)
project(weatherwar LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
find_package(SDL3 CONFIG REQUIRED)
find_package(Threads REQUIRED)
add_library(weatherwar_core
    src/assets/character_rom.cpp
    src/audio/replay_music.cpp
    src/basic/basic_input.cpp
    src/basic/basic_random.cpp
    src/basic/basic_value.cpp
    src/game/computer_player.cpp
    src/game/first_turn.cpp
    src/game/match_result.cpp
    src/game/scene_playback.cpp
    src/scenes/final_score.cpp
    src/scenes/opening.cpp
    src/scenes/setup.cpp
    src/scenes/statistics.cpp
    src/video/character_frame.cpp
    src/video/frame_export.cpp
    src/video/text_screen.cpp
    src/weather/attack_timing.cpp
    src/weather/nature_intro.cpp
    src/weather/weather_attack.cpp)
target_include_directories(weatherwar_core PUBLIC src)
set(SID_PREFIX "${CMAKE_SOURCE_DIR}/build/deps/libresidfp" CACHE PATH "Pinned SID library installation")
if(NOT EXISTS "${SID_PREFIX}/lib/libresidfp.a" OR NOT EXISTS "${SID_PREFIX}/SOURCE.txt")
    message(FATAL_ERROR "Missing pinned SID library. Run make audio-setup.")
endif()
file(STRINGS "${SID_PREFIX}/SOURCE.txt" SID_PROVENANCE)
foreach(SID_REQUIRED_LINE
    "revision=7c54a5988f9f1918439ee1180316a78c7a7729bb"
    "source=upstream")
    list(FIND SID_PROVENANCE "${SID_REQUIRED_LINE}" SID_PROVENANCE_INDEX)
    if(SID_PROVENANCE_INDEX EQUAL -1)
        message(FATAL_ERROR "SID source revision mismatch. Run make audio-setup.")
    endif()
endforeach()
add_library(residfp STATIC IMPORTED)
set_target_properties(residfp PROPERTIES
    IMPORTED_LOCATION "${SID_PREFIX}/lib/libresidfp.a"
    INTERFACE_INCLUDE_DIRECTORIES "${SID_PREFIX}/include"
    INTERFACE_LINK_LIBRARIES Threads::Threads)
add_library(weatherwar_audio src/audio/sid_chip.cpp src/audio/startup_sequence.cpp src/audio/sid_timeline.cpp)
target_include_directories(weatherwar_audio PUBLIC src)
target_link_libraries(weatherwar_audio PRIVATE residfp)
add_executable(weatherwar src/main.cpp)
option(WEATHERWAR_RELEASE "Build a ZIP with statically linked SDL" OFF)
if(WEATHERWAR_RELEASE)
    target_link_libraries(weatherwar PRIVATE SDL3::SDL3-static weatherwar_core weatherwar_audio)
    include(cmake/Release.cmake)
else()
    target_link_libraries(weatherwar PRIVATE SDL3::SDL3 weatherwar_core weatherwar_audio)
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

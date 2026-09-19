find_package(Python3 REQUIRED COMPONENTS Interpreter)

function(add_native_test source)
    string(REPLACE "_test.cpp" "" test_name "${source}")
    string(REPLACE "/" "_" target "${test_name}_test")
    add_executable(${target} "${source}")
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${target} PRIVATE ${ARGN})
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME "${test_name}" COMMAND ${target})
endfunction()

add_native_test(audio/replay_music_test.cpp weatherwar_core weatherwar_audio)
add_native_test(audio/sid_timeline_test.cpp weatherwar_audio)
add_native_test(audio/sid_timeline_transitions_test.cpp weatherwar_audio)
add_native_test(audio/startup_sequence_test.cpp weatherwar_audio)
add_native_test(audio/startup_sequence_tones_test.cpp weatherwar_audio)

add_native_test(basic/basic_input_test.cpp weatherwar_core)
add_native_test(basic/basic_random_test.cpp weatherwar_core)
add_native_test(basic/basic_value_test.cpp weatherwar_core)

add_native_test(game/computer_player_test.cpp weatherwar_core)
add_native_test(game/first_turn_test.cpp weatherwar_core)
add_native_test(game/match_result_test.cpp weatherwar_core)
add_native_test(game/scene_playback_test.cpp weatherwar_core weatherwar_audio)

add_native_test(integration/first_turn_match_test.cpp weatherwar_core)

add_native_test(scenes/final_score_test.cpp weatherwar_core)
add_native_test(scenes/opening_test.cpp weatherwar_core)
add_native_test(scenes/setup_test.cpp weatherwar_core)
add_native_test(scenes/statistics_test.cpp weatherwar_core)

add_native_test(video/character_frame_test.cpp weatherwar_core)
add_native_test(video/text_screen_test.cpp weatherwar_core)

add_native_test(weather/nature_intro_test.cpp weatherwar_core)
add_native_test(weather/weather_attack_test.cpp weatherwar_core)

add_executable(native_dump "${PROJECT_SOURCE_DIR}/tools/native_dump.cpp")
target_link_libraries(native_dump PRIVATE weatherwar_core weatherwar_audio)
add_executable(audio_transition_check "${PROJECT_SOURCE_DIR}/tools/audio_transition_check.cpp")
target_link_libraries(audio_transition_check PRIVATE weatherwar_audio)

function(add_tool_test name script)
    add_test(NAME "${name}" COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/${script}" ${ARGN})
endfunction()
add_tool_test(integration/audio_transitions check_audio_transitions.py
    --renderer "$<TARGET_FILE:audio_transition_check>"
    --executable "$<TARGET_FILE:native_dump>")
add_tool_test(integration/audio_consistency check_audio_consistency.py
    --executable "$<TARGET_FILE:native_dump>")
add_tool_test(integration/scene_audio check_scene_audio.py
    --executable "$<TARGET_FILE:native_dump>")
add_tool_test(integration/native_regression check_regression.py
    --executable "$<TARGET_FILE:native_dump>")
set_tests_properties(integration/audio_transitions integration/native_regression
    PROPERTIES TIMEOUT 180)

foreach(script IN ITEMS test_setup_audio test_check_scene_audio test_package_linux)
    string(REGEX REPLACE "^test_" "" name "${script}")
    add_test(NAME "tools/${name}" COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/${script}.py")
endforeach()

add_test(NAME integration/sdl_smoke COMMAND weatherwar --smoke-test)
set_tests_properties(integration/sdl_smoke PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy"
    TIMEOUT 10)

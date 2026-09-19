# Release packaging for the current host.

include(CheckIPOSupported)
check_ipo_supported()
set_property(TARGET weatherwar weatherwar_core weatherwar_audio
    PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

if(WIN32)
    set_target_properties(weatherwar PROPERTIES WIN32_EXECUTABLE TRUE)
    set(_weatherwar_release_platform Windows)
elseif(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0)
    endif()
    set_target_properties(weatherwar PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Weatherwar II"
        MACOSX_BUNDLE_BUNDLE_NAME "Weatherwar II"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.weatherwar.game"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/cmake/WeatherwarInfo.plist.in")
    set(_weatherwar_release_platform macOS)
else()
    set(_weatherwar_release_platform "${CMAKE_SYSTEM_NAME}")
endif()

set(_weatherwar_release_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(_weatherwar_release_arch MATCHES "^(AMD64|amd64|X64|x64|X86_64|x86_64)$")
    set(_weatherwar_release_arch x86_64)
elseif(_weatherwar_release_arch MATCHES "^(ARM64|arm64|AARCH64|aarch64)$")
    set(_weatherwar_release_arch arm64)
endif()

set(WEATHERWAR_RELEASE_DIR "${PROJECT_SOURCE_DIR}/build/release" CACHE PATH
    "Directory receiving the final Weatherwar release ZIP")
set(_weatherwar_release_stage "${CMAKE_BINARY_DIR}/_weatherwar_release_stage")
set(_weatherwar_release_readme "${CMAKE_BINARY_DIR}/weatherwar-release-README.txt")
set(_weatherwar_release_name
    "weatherwar-${_weatherwar_release_platform}-${_weatherwar_release_arch}")
set(_weatherwar_release_archive
    "${_weatherwar_release_stage}/${_weatherwar_release_name}.zip")

if(WIN32)
    set(_weatherwar_release_readme_content
"Weatherwar II
Windows release.

Run weatherwar.exe from this folder.

Return confirms input; Backspace edits. Choose H, L, R or T and enter charge.
F11 toggles fullscreen; Escape returns to the window, then exits.

The bundled archive includes the non-system runtime DLLs. Your graphics and audio drivers remain required.

")
elseif(APPLE)
    set(_weatherwar_release_readme_content
"Weatherwar II
macOS release.

Open Weatherwar II.app in Finder, or run: open \"Weatherwar II.app\"

Return confirms input; Backspace edits. Choose H, L, R or T and enter charge.
F11 toggles fullscreen; Escape returns to the window, then exits.

This build is ad-hoc signed for local use and is not notarized. macOS may ask you to approve it on first launch.

")
else()
    set(_weatherwar_release_readme_content
"Weatherwar II
Native port of the Commodore 64 game.

Run ./weatherwar on the host system. This archive targets ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}; SDL3 is statically linked, while host operating-system libraries remain required.

Return confirms input; Backspace edits. Choose H, L, R or T and enter charge.
F11 toggles fullscreen; Escape returns to the window, then exits.

")
endif()

file(GENERATE OUTPUT "${_weatherwar_release_readme}" CONTENT
    "${_weatherwar_release_readme_content}")

if(APPLE)
    install(TARGETS weatherwar BUNDLE DESTINATION .)
else()
    install(TARGETS weatherwar RUNTIME DESTINATION .)
endif()
install(FILES "${_weatherwar_release_readme}" DESTINATION . RENAME README.txt)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR ZIP)
    set(CPACK_PACKAGE_NAME weatherwar)
    set(CPACK_PACKAGE_FILE_NAME "${_weatherwar_release_name}")
    set(CPACK_PACKAGE_DIRECTORY "${_weatherwar_release_stage}")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    set(CPACK_STRIP_FILES TRUE)
    include(CPack)
endif()

set(_weatherwar_bundle_commands)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    list(APPEND _weatherwar_bundle_commands
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/package_linux.py"
            "$<TARGET_FILE:weatherwar>"
            "${WEATHERWAR_RELEASE_DIR}/${_weatherwar_release_name}-bundled.zip"
            --strip "${CMAKE_STRIP}")
elseif(WIN32)
    set(_weatherwar_windows_stage "${CMAKE_BINARY_DIR}/_weatherwar_windows_stage")
    list(APPEND _weatherwar_bundle_commands
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_weatherwar_windows_stage}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_weatherwar_windows_stage}"
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
                --prefix "${_weatherwar_windows_stage}"
        COMMAND "${CMAKE_COMMAND}"
            "-DWEATHERWAR_EXECUTABLE=${_weatherwar_windows_stage}/weatherwar.exe"
            "-DWEATHERWAR_STAGE=${_weatherwar_windows_stage}"
            "-DWEATHERWAR_OUTPUT=${WEATHERWAR_RELEASE_DIR}/${_weatherwar_release_name}-bundled.zip"
            "-DWEATHERWAR_STRIP=${CMAKE_STRIP}"
            "-DWEATHERWAR_OBJDUMP=${CMAKE_OBJDUMP}"
            -P "${PROJECT_SOURCE_DIR}/cmake/BundleWindows.cmake")
elseif(APPLE)
    set(_weatherwar_macos_stage "${CMAKE_BINARY_DIR}/_weatherwar_macos_stage")
    list(APPEND _weatherwar_bundle_commands
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_weatherwar_macos_stage}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_weatherwar_macos_stage}"
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
                --prefix "${_weatherwar_macos_stage}"
        COMMAND "${CMAKE_COMMAND}"
            "-DWEATHERWAR_APP=${_weatherwar_macos_stage}/Weatherwar II.app"
            "-DWEATHERWAR_STAGE=${_weatherwar_macos_stage}"
            "-DWEATHERWAR_OUTPUT=${WEATHERWAR_RELEASE_DIR}/${_weatherwar_release_name}-bundled.zip"
            "-DWEATHERWAR_STRIP=${CMAKE_STRIP}"
            -P "${PROJECT_SOURCE_DIR}/cmake/BundleMacOS.cmake")
endif()

set(_weatherwar_release_commands
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_weatherwar_release_stage}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${WEATHERWAR_RELEASE_DIR}")
set(_weatherwar_release_byproducts)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND _weatherwar_release_commands
        COMMAND "${CMAKE_CPACK_COMMAND}" --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_weatherwar_release_archive}"
                "${WEATHERWAR_RELEASE_DIR}/${_weatherwar_release_name}.zip")
    list(APPEND _weatherwar_release_byproducts
        "${WEATHERWAR_RELEASE_DIR}/${_weatherwar_release_name}.zip")
endif()
if(WIN32 OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND _weatherwar_release_byproducts
        "${WEATHERWAR_RELEASE_DIR}/${_weatherwar_release_name}-bundled.zip")
endif()

add_custom_target(release
    ${_weatherwar_release_commands}
    ${_weatherwar_bundle_commands}
    DEPENDS weatherwar
    BYPRODUCTS ${_weatherwar_release_byproducts}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    VERBATIM)

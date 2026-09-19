cmake_minimum_required(VERSION 3.20)

foreach(_weatherwar_required WEATHERWAR_APP WEATHERWAR_STAGE WEATHERWAR_OUTPUT)
    if(NOT DEFINED ${_weatherwar_required} OR
       "${${_weatherwar_required}}" STREQUAL "")
        message(FATAL_ERROR "${_weatherwar_required} is required")
    endif()
endforeach()

if(NOT EXISTS "${WEATHERWAR_APP}")
    message(FATAL_ERROR "macOS app bundle does not exist: ${WEATHERWAR_APP}")
endif()

include(BundleUtilities)
get_bundle_main_executable("${WEATHERWAR_APP}" _weatherwar_executable)
if(_weatherwar_executable MATCHES "^error:")
    message(FATAL_ERROR "Could not find macOS bundle executable: ${_weatherwar_executable}")
endif()

if(DEFINED WEATHERWAR_STRIP AND NOT "${WEATHERWAR_STRIP}" STREQUAL ""
   AND NOT WEATHERWAR_STRIP MATCHES "-NOTFOUND$")
    execute_process(
        COMMAND "${WEATHERWAR_STRIP}" -x "${_weatherwar_executable}"
        RESULT_VARIABLE _weatherwar_strip_result
        OUTPUT_VARIABLE _weatherwar_strip_output
        ERROR_VARIABLE _weatherwar_strip_error)
    if(NOT _weatherwar_strip_result EQUAL 0)
        message(FATAL_ERROR
            "Could not strip macOS executable: ${_weatherwar_strip_error}")
    endif()
endif()

# fixup_bundle copies non-system dylibs into Contents/Frameworks and updates
# their install names. Apple frameworks and system libraries stay on the host.
fixup_bundle("${WEATHERWAR_APP}" "" "${WEATHERWAR_STAGE}")

find_program(_weatherwar_codesign codesign)
if(NOT _weatherwar_codesign)
    message(FATAL_ERROR "codesign is required to create the macOS release")
endif()
execute_process(
    COMMAND "${_weatherwar_codesign}" --deep --force --sign - "${WEATHERWAR_APP}"
    RESULT_VARIABLE _weatherwar_codesign_result
    OUTPUT_VARIABLE _weatherwar_codesign_output
    ERROR_VARIABLE _weatherwar_codesign_error)
if(NOT _weatherwar_codesign_result EQUAL 0)
    message(FATAL_ERROR "Could not ad-hoc sign macOS app: ${_weatherwar_codesign_error}")
endif()

if(NOT EXISTS "${WEATHERWAR_STAGE}/README.txt")
    message(FATAL_ERROR "macOS release stage has no README.txt")
endif()

file(GLOB _weatherwar_archive_entries RELATIVE "${WEATHERWAR_STAGE}"
    "${WEATHERWAR_STAGE}/*")
if(NOT _weatherwar_archive_entries)
    message(FATAL_ERROR "macOS release stage is empty")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${WEATHERWAR_OUTPUT}"
            --format=zip ${_weatherwar_archive_entries}
    WORKING_DIRECTORY "${WEATHERWAR_STAGE}"
    RESULT_VARIABLE _weatherwar_archive_result
    ERROR_VARIABLE _weatherwar_archive_error)
if(NOT _weatherwar_archive_result EQUAL 0)
    message(FATAL_ERROR "Could not create macOS ZIP: ${_weatherwar_archive_error}")
endif()

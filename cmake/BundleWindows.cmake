cmake_minimum_required(VERSION 3.20)

if(DEFINED WEATHERWAR_OBJDUMP AND NOT "${WEATHERWAR_OBJDUMP}" STREQUAL ""
   AND NOT WEATHERWAR_OBJDUMP MATCHES "-NOTFOUND$")
    set(CMAKE_OBJDUMP "${WEATHERWAR_OBJDUMP}")
endif()
# This release uses MinGW's PE parser even when a Windows SDK is installed.
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM windows+pe)
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL objdump)
if(DEFINED WEATHERWAR_OBJDUMP AND NOT "${WEATHERWAR_OBJDUMP}" STREQUAL ""
   AND NOT WEATHERWAR_OBJDUMP MATCHES "-NOTFOUND$")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${WEATHERWAR_OBJDUMP}")
endif()

foreach(_weatherwar_required
        WEATHERWAR_EXECUTABLE WEATHERWAR_STAGE WEATHERWAR_OUTPUT)
    if(NOT DEFINED ${_weatherwar_required} OR
       "${${_weatherwar_required}}" STREQUAL "")
        message(FATAL_ERROR "${_weatherwar_required} is required")
    endif()
endforeach()

if(NOT EXISTS "${WEATHERWAR_EXECUTABLE}")
    message(FATAL_ERROR "Windows executable does not exist: ${WEATHERWAR_EXECUTABLE}")
endif()

file(MAKE_DIRECTORY "${WEATHERWAR_STAGE}")

if(DEFINED WEATHERWAR_STRIP AND NOT "${WEATHERWAR_STRIP}" STREQUAL ""
   AND NOT WEATHERWAR_STRIP MATCHES "-NOTFOUND$")
    execute_process(
        COMMAND "${WEATHERWAR_STRIP}" "${WEATHERWAR_EXECUTABLE}"
        RESULT_VARIABLE _weatherwar_strip_result
        OUTPUT_VARIABLE _weatherwar_strip_output
        ERROR_VARIABLE _weatherwar_strip_error)
    if(NOT _weatherwar_strip_result EQUAL 0)
        message(FATAL_ERROR
            "Could not strip Windows executable: ${_weatherwar_strip_error}")
    endif()
endif()

# Keep Windows system DLLs and API-set contracts on the host. Everything else
# found by the dependency walker is copied beside weatherwar.exe.
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${WEATHERWAR_EXECUTABLE}"
    RESOLVED_DEPENDENCIES_VAR _weatherwar_resolved
    UNRESOLVED_DEPENDENCIES_VAR _weatherwar_unresolved
    PRE_EXCLUDE_REGEXES
        "^[Aa][Pp][Ii]-[Mm][Ss]-[Ww][Ii][Nn]-.*"
        "^[Ee][Xx][Tt]-[Mm][Ss]-[Ww][Ii][Nn]-.*"
    POST_EXCLUDE_REGEXES
        ".*[\\/][Ww][Ii][Nn][Dd][Oo][Ww][Ss][\\/].*"
        ".*[\\/][Ww][Ii][Nn][Dd][Oo][Ww][Ss][\\/][Ss][Yy][Ss][Tt][Ee][Mm]32[\\/].*")

if(_weatherwar_unresolved)
    string(JOIN ", " _weatherwar_missing ${_weatherwar_unresolved})
    message(FATAL_ERROR
        "Unresolved non-system Windows runtime dependencies: ${_weatherwar_missing}")
endif()

if(NOT EXISTS "${WEATHERWAR_STAGE}/README.txt")
    message(FATAL_ERROR "Windows release stage has no README.txt")
endif()

foreach(_weatherwar_dependency IN LISTS _weatherwar_resolved)
    file(COPY "${_weatherwar_dependency}" DESTINATION "${WEATHERWAR_STAGE}")
endforeach()

file(GLOB _weatherwar_archive_entries RELATIVE "${WEATHERWAR_STAGE}"
    "${WEATHERWAR_STAGE}/*")
if(NOT _weatherwar_archive_entries)
    message(FATAL_ERROR "Windows release stage is empty")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${WEATHERWAR_OUTPUT}"
            --format=zip ${_weatherwar_archive_entries}
    WORKING_DIRECTORY "${WEATHERWAR_STAGE}"
    RESULT_VARIABLE _weatherwar_archive_result
    ERROR_VARIABLE _weatherwar_archive_error)
if(NOT _weatherwar_archive_result EQUAL 0)
    message(FATAL_ERROR "Could not create Windows ZIP: ${_weatherwar_archive_error}")
endif()

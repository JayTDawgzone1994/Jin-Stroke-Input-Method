cmake_minimum_required(VERSION 3.25)
project(StrokeIME VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/ProjectOptions.cmake)
include(CTest)
option(STROKE_BUILD_TOOLS "Build development tool entry points" ON)

add_subdirectory(src/domain)
add_subdirectory(src/dictionary)
add_subdirectory(src/engine)
add_subdirectory(src/windows)
if(STROKE_BUILD_TOOLS)
    if(WIN32 AND MSVC)
        add_subdirectory(tools/icon_builder)
    endif()
    add_subdirectory(tools/dict_builder)
    add_subdirectory(tools/engine_console)
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# System registration is an explicit setup operation, never part of the build.

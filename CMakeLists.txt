cmake_minimum_required(VERSION 3.22)

project(DevKomodoPedals VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEVKOMODO_DEMO_BUILD "Build the time-limited commercial demo" OFF)
if(DEVKOMODO_DEMO_BUILD)
    add_compile_definitions(DEVKOMODO_DEMO_BUILD=1)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Keep the package lean for GitHub runners and shipping builds.
# LTO is enabled per JUCE target below; enabling global IPO as well can
# duplicate/conflict with JUCE's recommended LTO flags on MSVC.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

# Keep the active build focused on the wavetable synth until the remaining
# plugin targets are ready for full CI coverage.
if(WIN32)
    set(DEVKOMODO_PLUGIN_FORMATS VST3)
else()
    set(DEVKOMODO_PLUGIN_FORMATS VST3)
endif()

# Descarga JUCE una sola vez para el target activo.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Build all plugin targets in the Pedals directory.
file(GLOB DEVKOMODO_PEDAL_DIRS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/Pedals" "${CMAKE_CURRENT_SOURCE_DIR}/Pedals/*")
foreach(pedal_dir IN LISTS DEVKOMODO_PEDAL_DIRS)
    set(pedal_path "${CMAKE_CURRENT_SOURCE_DIR}/Pedals/${pedal_dir}")
    if(IS_DIRECTORY "${pedal_path}" AND EXISTS "${pedal_path}/CMakeLists.txt")
        add_subdirectory("${pedal_path}")
    endif()
endforeach()


cmake_minimum_required(VERSION 3.22)

project(DevKomodoPedals VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library" FORCE)
endif()

option(DEVKOMODO_DEMO_BUILD "Build the time-limited commercial demo" OFF)
if(DEVKOMODO_DEMO_BUILD)
    add_compile_definitions(DEVKOMODO_DEMO_BUILD=1)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "DevKomodo VST3 plugins must be built as 64-bit for FL Studio and modern DAWs. "
        "Reconfigure with: cmake -S . -B build -G \"Visual Studio 17 2022\" -A x64"
    )
endif()

# Keep the package lean for GitHub runners and shipping builds.
# LTO is enabled per JUCE target below; enabling global IPO as well can
# duplicate/conflict with JUCE's recommended LTO flags on MSVC.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

# Keep the active build focused on the current instrument target for lean CI.
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

# Build only the active plugin target.
set(DEVKOMODO_ACTIVE_PLUGIN "JunoEmu" CACHE STRING "Plugin target folder in Pedals/")
set(_devkomodo_active_plugin_dir "${CMAKE_CURRENT_SOURCE_DIR}/Pedals/${DEVKOMODO_ACTIVE_PLUGIN}")

if(NOT EXISTS "${_devkomodo_active_plugin_dir}/CMakeLists.txt")
    message(FATAL_ERROR
        "DEVKOMODO_ACTIVE_PLUGIN='${DEVKOMODO_ACTIVE_PLUGIN}' is invalid. "
        "Expected ${_devkomodo_active_plugin_dir}/CMakeLists.txt to exist."
    )
endif()

add_subdirectory("${_devkomodo_active_plugin_dir}")

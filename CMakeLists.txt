cmake_minimum_required(VERSION 3.22)

project(DevKomodoPedals VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Keep the package lean for GitHub runners and shipping builds.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This distribution is intentionally Windows-only. GitHub Actions and release
# artifacts target 64-bit Windows VST3 with the MSVC toolchain.
if(NOT WIN32)
    message(FATAL_ERROR "DevKomodo Pedals is Windows-only and builds VST3 only.")
endif()

# Descarga JUCE UNA sola vez; todos los pedales de abajo la comparten.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Cada pedal es un subproyecto independiente con su propio target/plugin.
add_subdirectory(Pedals/Overdrive)
add_subdirectory(Pedals/Chorus)
add_subdirectory(Pedals/Reverb)
add_subdirectory(Pedals/SynthGuitar)
add_subdirectory(Pedals/SynthBass)
add_subdirectory(Pedals/FuzzGuitar)
add_subdirectory(Pedals/DistortionGuitar)
add_subdirectory(Pedals/Compressor)
add_subdirectory(Pedals/Tremolo)
add_subdirectory(Pedals/Phaser)
add_subdirectory(Pedals/Delay)
add_subdirectory(Pedals/NoiseGate)
add_subdirectory(Pedals/Boost)
add_subdirectory(Pedals/RingModulator)
add_subdirectory(Pedals/EQ)
add_subdirectory(Pedals/EnvelopeFilterGuitar)
add_subdirectory(Pedals/OctaverGuitar)
add_subdirectory(Pedals/MultibandCompressor)
add_subdirectory(Pedals/Preamp)
add_subdirectory(Pedals/Saturator)
add_subdirectory(Pedals/VocalShifter)
add_subdirectory(Pedals/Flanger)
add_subdirectory(Pedals/AmpSim)
add_subdirectory(Pedals/Bitcrusher)
add_subdirectory(Pedals/GraphicEQ)
add_subdirectory(Pedals/Deesser)
add_subdirectory(Pedals/TransientShaper)
add_subdirectory(Pedals/Doubler)
add_subdirectory(Pedals/DrumEnhancer)
add_subdirectory(Pedals/BroadcastCompressor)
add_subdirectory(Pedals/TapeEmulation)
add_subdirectory(Pedals/VinylEmulation)
add_subdirectory(Pedals/ConvolutionReverb)
add_subdirectory(Pedals/Exciter)
add_subdirectory(Pedals/AirEnhancer)
add_subdirectory(Pedals/MonoMaker)
add_subdirectory(Pedals/PhaseAlign)
add_subdirectory(Pedals/AutoSwell)
add_subdirectory(Pedals/Vibrato)
add_subdirectory(Pedals/RotarySpeaker)
add_subdirectory(Pedals/SpringReverb)
add_subdirectory(Pedals/TapeDelay)
add_subdirectory(Pedals/GlitchMachine)
add_subdirectory(Pedals/Granulator)
add_subdirectory(Pedals/Limiter)
add_subdirectory(Pedals/Clipper)
add_subdirectory(Pedals/ConsoleEQ)
add_subdirectory(Pedals/UtilityGain)
add_subdirectory(Pedals/ToneSculptor)
add_subdirectory(Pedals/ConsoleDrive)
add_subdirectory(Pedals/CleanUpPro)


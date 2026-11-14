cmake_minimum_required(VERSION 3.22)

# macOS 11 (Big Sur) on Intel is the minimum; build universal (x86_64 + arm64) on CI.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # static CRT: no VC++ redist needed
endif()
project(Kaleido VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Core engine (no JUCE dependency, unit-testable) ----
add_library(kaleido_engine STATIC
    Source/engine/ArpTypes.cpp
    Source/engine/ArpEngine.cpp
    Source/engine/Generators.cpp
    Source/engine/Presets.cpp)
target_include_directories(kaleido_engine PUBLIC Source/engine)
set_target_properties(kaleido_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(KALEIDO_BUILD_TESTS "Build engine tests" ON)
if(KALEIDO_BUILD_TESTS)
    add_executable(kaleido_tests tests/EngineTests.cpp)
    target_link_libraries(kaleido_tests PRIVATE kaleido_engine)
    enable_testing()
    add_test(NAME engine COMMAND kaleido_tests)
endif()

option(KALEIDO_BUILD_PLUGIN "Build the JUCE plugin" ON)
if(NOT KALEIDO_BUILD_PLUGIN)
    return()
endif()

# ---- JUCE ----
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(KALEIDO_PLUGIN_SOURCES
    Source/plugin/PluginProcessor.cpp
    Source/plugin/PluginEditor.cpp
    Source/plugin/Themes.cpp)

function(kaleido_configure target)
    target_sources(${target} PRIVATE ${KALEIDO_PLUGIN_SOURCES})
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${target}
        PRIVATE
            kaleido_engine
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

# Main build: loads as an instrument with MIDI out in every VST3 host (route its MIDI to
# another track), plus AU on macOS and a standalone app with MIDI in/out device selection.
juce_add_plugin(Kaleido
    COMPANY_NAME "Kaleido Audio"
    PLUGIN_MANUFACTURER_CODE Kldo
    PLUGIN_CODE Karp
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Kaleido"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Generator)
kaleido_configure(Kaleido)

# Logic Pro's MIDI FX slot needs a dedicated AU MIDI-effect ("aumi") build.
if(APPLE)
    juce_add_plugin(KaleidoMidiFX
        COMPANY_NAME "Kaleido Audio"
        PLUGIN_MANUFACTURER_CODE Kldo
        PLUGIN_CODE Kmfx
        FORMATS AU
        PRODUCT_NAME "Kaleido MIDI FX"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        AU_MAIN_TYPE kAudioUnitType_MIDIProcessor)
    kaleido_configure(KaleidoMidiFX)
endif()

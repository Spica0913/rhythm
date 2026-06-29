cmake_minimum_required(VERSION 3.22)

project(Rhythm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(RHYTHM_BUILD_PLUGIN "Build the JUCE plugin (downloads JUCE)" ON)
option(RHYTHM_BUILD_TESTS  "Build the core unit tests" ON)
option(RHYTHM_BUILD_CLI    "Build the rhythm_cli MIDI export tool" ON)

# ---------------------------------------------------------------------------
# Core music engine — pure C++, no JUCE dependency. Reusable & unit-testable.
# ---------------------------------------------------------------------------
add_library(rhythm_core STATIC
    src/core/Chord.cpp
    src/core/Voicing.cpp
    src/core/Pattern.cpp
    src/core/Progression.cpp
    src/core/Generator.cpp
    src/core/MidiFile.cpp)
target_include_directories(rhythm_core PUBLIC src/core)
target_compile_features(rhythm_core PUBLIC cxx_std_17)
set_target_properties(rhythm_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Unit tests (no external framework required).
# ---------------------------------------------------------------------------
if(RHYTHM_BUILD_TESTS)
    enable_testing()
    add_executable(rhythm_tests tests/test_core.cpp)
    target_link_libraries(rhythm_tests PRIVATE rhythm_core)
    add_test(NAME rhythm_core_tests COMMAND rhythm_tests)
endif()

# ---------------------------------------------------------------------------
# Command-line MIDI exporter.
# ---------------------------------------------------------------------------
if(RHYTHM_BUILD_CLI)
    add_executable(rhythm_cli tools/rhythm_cli.cpp)
    target_link_libraries(rhythm_cli PRIVATE rhythm_core)
endif()

# ---------------------------------------------------------------------------
# JUCE plugin.
# ---------------------------------------------------------------------------
if(RHYTHM_BUILD_PLUGIN)
    # Use a local JUCE checkout if JUCE_PATH is provided, otherwise fetch it.
    if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
        message(STATUS "Using local JUCE at ${JUCE_PATH}")
        add_subdirectory("${JUCE_PATH}" juce_build)
    else()
        message(STATUS "Fetching JUCE via FetchContent")
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.4
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    juce_add_plugin(RhythmPlugin
        PRODUCT_NAME            "Rhythm"
        COMPANY_NAME            "RhythmAudio"
        BUNDLE_ID               "com.rhythmaudio.rhythm"
        PLUGIN_MANUFACTURER_CODE Rhym
        PLUGIN_CODE             Rhy1
        # Hosted as an instrument that emits MIDI. Ableton Live / FL / Bitwig
        # have no third-party MIDI-effect slot, so a MIDI generator must load
        # as an instrument and have its MIDI output routed to another track.
        IS_SYNTH                TRUE
        NEEDS_MIDI_INPUT        TRUE
        NEEDS_MIDI_OUTPUT       TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD TRUE
        FORMATS                 VST3 Standalone AU)

    target_sources(RhythmPlugin PRIVATE
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp)

    target_compile_definitions(RhythmPlugin PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(RhythmPlugin PRIVATE
        rhythm_core
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif()

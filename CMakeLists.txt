cmake_minimum_required(VERSION 3.22)

project(Kilocycle
    VERSION 0.1.0
    DESCRIPTION "Kilocycle - a retro broadcast console channel strip"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(KILOCYCLE_COPY_AFTER_BUILD "Install the built plug-in into the user VST3 folder" ON)
option(KILOCYCLE_BUILD_TOOLS "Build the developer tool (offline DSP check / panel renderer)" OFF)

# Shown on the panel next to the version. CI passes the commit it built from, so a
# downloaded binary can always be traced back to its source. Deliberately separate
# from the plug-in version, which stays stable so hosts do not re-scan on every
# development build.
set(KILOCYCLE_BUILD_ID "" CACHE STRING "Build identifier shown in the UI (e.g. a commit SHA)")

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
# Vendored as a shallow clone in libs/JUCE (see README for the exact command).
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found in libs/JUCE.\n"
        "Run: git submodule update --init")
endif()

add_subdirectory(libs/JUCE)

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
juce_add_plugin(Kilocycle
    PRODUCT_NAME                "Kilocycle"
    COMPANY_NAME                "Kilocycle Audio"
    COMPANY_WEBSITE             "https://example.invalid/kilocycle"
    BUNDLE_ID                   "audio.kilocycle.console"
    PLUGIN_MANUFACTURER_CODE    Klcy
    PLUGIN_CODE                 Kcyc
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Fx Dynamics EQ Distortion
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${KILOCYCLE_COPY_AFTER_BUILD})

juce_generate_juce_header(Kilocycle)

target_sources(Kilocycle PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/gui/Skin.cpp
    Source/gui/RadioLookAndFeel.cpp
    Source/gui/RadioKnob.cpp
    Source/gui/VuMeter.cpp
    Source/gui/GainReductionMeter.cpp
    Source/gui/TuningDial.cpp
    Source/gui/ValveGlow.cpp
    Source/gui/RockerSwitch.cpp)

target_include_directories(Kilocycle PRIVATE Source)

if (KILOCYCLE_BUILD_ID)
    string(SUBSTRING "${KILOCYCLE_BUILD_ID}" 0 7 KILOCYCLE_BUILD_ID_SHORT)
    target_compile_definitions(Kilocycle PRIVATE
        KILOCYCLE_BUILD_ID="${KILOCYCLE_BUILD_ID_SHORT}")
endif()

target_compile_definitions(Kilocycle PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(Kilocycle
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# A few extra optimisation flags for the DSP-heavy translation units.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(Kilocycle PRIVATE
        $<$<CONFIG:Release>:-O3 -fno-math-errno>)
endif()

# ---------------------------------------------------------------------------
# Developer tool: offline DSP checks and a headless panel renderer.
# Handy in CI - neither mode needs an audio device or a window.
# ---------------------------------------------------------------------------
if (KILOCYCLE_BUILD_TOOLS)
    juce_add_console_app(KilocycleDevTool PRODUCT_NAME "kilocycle-devtool")

    target_sources(KilocycleDevTool PRIVATE tools/DevTool.cpp)
    target_include_directories(KilocycleDevTool PRIVATE Source)

    target_link_libraries(KilocycleDevTool PRIVATE
        Kilocycle
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    target_compile_definitions(KilocycleDevTool PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=0)
endif()

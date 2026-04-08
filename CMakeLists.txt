cmake_minimum_required(VERSION 3.22)
# If CMake errors about CMakeCache.txt living in a different directory than this
# source tree, remove the build folder and configure again (stale cache from another path).
if(APPLE)
    project(PatternFlow VERSION 1.0.1 LANGUAGES C CXX OBJCXX)
else()
    project(PatternFlow VERSION 1.0.1 LANGUAGES C CXX)
endif()

set(PF_VERSION "${PROJECT_VERSION}")
set(GIT_HASH "unknown")
set(GIT_DATE "unknown")
set(GIT_SUBJECT "dev build")
find_program(GIT_EXECUTABLE git)
if(GIT_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    # Commit time in America/Los_Angeles (PST/PDT). %ci ignores TZ; iso-local does not.
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "TZ=America/Los_Angeles"
            "${GIT_EXECUTABLE}" log -1 --format=%cd --date=iso-local
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" log -1 --format=%s
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_SUBJECT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Generated")
configure_file(
    "${CMAKE_SOURCE_DIR}/Source/BuildInfo.h.in"
    "${CMAKE_BINARY_DIR}/Generated/BuildInfo.h"
    @ONLY)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")

# Workaround: Xcode 26 / macOS 26 SDK marks older CoreGraphics APIs as
# unavailable.  Cap the SDK version the headers advertise so JUCE can still
# compile until upstream adds macOS 26 support.
# Set via CMAKE_*_FLAGS so the flag propagates to all sub-builds (juceaide).
if(APPLE)
    set(_sdk_compat "-DMAC_OS_X_VERSION_MIN_REQUIRED=130000 -DMAC_OS_X_VERSION_MAX_ALLOWED=150400")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_sdk_compat}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_sdk_compat}")
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} ${_sdk_compat}")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} ${_sdk_compat}")
endif()

# ── JUCE (fetched automatically) ──────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
)
FetchContent_MakeAvailable(JUCE)

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(PatternFlow
    COMPANY_NAME            "PatternFlow"
    PLUGIN_MANUFACTURER_CODE Ptfl
    PLUGIN_CODE              PtFl
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "PatternFlow"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    IS_MIDI_EFFECT           TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  TRUE
)

target_include_directories(PatternFlow PRIVATE "${CMAKE_BINARY_DIR}/Generated")

target_sources(PatternFlow PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MidiFileData.cpp
    Source/CompingModel.cpp
    Source/FileBrowserPanel.cpp
    Source/ArrangementView.cpp
    Source/PianoRollEditor.cpp
    Source/ControlPanel.cpp
    Source/Theme.cpp
    Source/UndoActions.cpp
)

target_compile_definitions(PatternFlow PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_ALSA=0
)

target_link_libraries(PatternFlow
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ── Unit tests (Catch2) ───────────────────────────────────────────────────────
option(PATTERNFLOW_BUILD_TESTS "Build unit tests" ON)

if(PATTERNFLOW_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.4
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(PatternFlowTests Tests/main.cpp)
    target_link_libraries(PatternFlowTests PRIVATE
        PatternFlow
        Catch2::Catch2WithMain
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_data_structures
    )
    target_include_directories(PatternFlowTests PRIVATE ${CMAKE_SOURCE_DIR}/Source "${CMAKE_BINARY_DIR}/Generated")
    include(CTest)
    enable_testing()
    add_test(NAME PatternFlowTests COMMAND PatternFlowTests)
endif()

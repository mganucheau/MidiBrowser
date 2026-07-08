cmake_minimum_required(VERSION 3.22)
if(APPLE)
    project(MidiBrowser VERSION 1.0.0 LANGUAGES C CXX OBJCXX)
else()
    project(MidiBrowser VERSION 1.0.0 LANGUAGES C CXX)
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

if(APPLE)
    set(_sdk_compat "-DMAC_OS_X_VERSION_MIN_REQUIRED=130000 -DMAC_OS_X_VERSION_MAX_ALLOWED=150400")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_sdk_compat}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_sdk_compat}")
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} ${_sdk_compat}")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} ${_sdk_compat}")
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(_midibrowser_formats AU VST3 Standalone)
else()
    set(_midibrowser_formats VST3 Standalone)
endif()

# Single instrument-style plugin: MIDI output + silent stereo audio.
# Loads reliably in Ableton (VST3) and Logic (AU). IS_MIDI_EFFECT breaks many hosts.
juce_add_plugin(MidiBrowser
    COMPANY_NAME            "MidiBrowser"
    BUNDLE_ID               com.midibrowser.app
    PLUGIN_MANUFACTURER_CODE MdbR
    PLUGIN_CODE              MdbB
    FORMATS                  ${_midibrowser_formats}
    PRODUCT_NAME             "Midi Browser"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  TRUE
)

juce_add_binary_data(MidiBrowserAssets
    SOURCES
        Resources/Fonts/SchibstedGrotesk.ttf
        Resources/Fonts/JetBrainsMono-Regular.ttf
        Resources/Fonts/JetBrainsMono-SemiBold.ttf
)

target_include_directories(MidiBrowser PRIVATE "${CMAKE_BINARY_DIR}/Generated")

target_sources(MidiBrowser PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MidiFileData.cpp
    Source/EditModel.cpp
    Source/GrooveEngine.cpp
    Source/UiAtoms.cpp
    Source/TransportBar.cpp
    Source/BrowserPanels.cpp
    Source/KnobPanel.cpp
    Source/PianoRollEditor.cpp
    Source/Theme.cpp
)

target_compile_definitions(MidiBrowser PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_ALSA=0
)

target_link_libraries(MidiBrowser
    PRIVATE
        MidiBrowserAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

option(MIDIBROWSER_BUILD_TESTS "Build unit tests" ON)
option(MIDIBROWSER_BUILD_PROBE "Build the VST3 MIDI-out probe tool" ON)

if(MIDIBROWSER_BUILD_PROBE)
    juce_add_console_app(MidiOutProbe PRODUCT_NAME "MidiOutProbe")
    target_sources(MidiOutProbe PRIVATE Tools/MidiOutProbe.cpp)
    target_compile_definitions(MidiOutProbe PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(MidiOutProbe PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )
endif()

if(MIDIBROWSER_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.4
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(MidiBrowserTests
        Tests/main.cpp
        Tests/MidiFileDataTests.cpp
        Tests/TrimTests.cpp
        Tests/PluginProcessorTests.cpp
        Tests/EditModelTests.cpp
        Tests/GrooveEngineTests.cpp
    )
    target_link_libraries(MidiBrowserTests PRIVATE
        MidiBrowser
        Catch2::Catch2WithMain
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_data_structures
    )
    target_include_directories(MidiBrowserTests PRIVATE ${CMAKE_SOURCE_DIR}/Source "${CMAKE_BINARY_DIR}/Generated")
    include(CTest)
    enable_testing()
    add_test(NAME MidiBrowserTests COMMAND MidiBrowserTests)
endif()

cmake_minimum_required(VERSION 3.22)
project(PatternFlow VERSION 1.0.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")

# Workaround: Xcode 26 / macOS 26 SDK marks older CoreGraphics APIs as
# unavailable.  Cap the SDK version the headers advertise so JUCE can still
# compile until upstream adds macOS 26 support.
if(APPLE)
    add_compile_options(-DMAC_OS_X_VERSION_MAX_ALLOWED=150400)
endif()

# ── JUCE (fetched automatically) ──────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        develop
)
FetchContent_MakeAvailable(JUCE)

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(PatternFlow
    COMPANY_NAME            "PatternFlow"
    PLUGIN_MANUFACTURER_CODE Ptfl
    PLUGIN_CODE              PtFl
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "PatternFlow"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    IS_MIDI_EFFECT           TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

target_sources(PatternFlow PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MidiFileData.cpp
    Source/FileBrowserPanel.cpp
    Source/ArrangementView.cpp
    Source/PianoRollEditor.cpp
    Source/ControlPanel.cpp
    Source/Theme.cpp
)

target_compile_definitions(PatternFlow PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(PatternFlow
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

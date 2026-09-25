cmake_minimum_required(VERSION 3.22)
# Pre-1.0 beta — each build appends an incrementing -beta.N via GenerateBuildInfo.
if(APPLE)
    project(MidiBrowser VERSION 0.1.0 LANGUAGES C CXX OBJCXX)
else()
    project(MidiBrowser VERSION 0.1.0 LANGUAGES C CXX)
endif()

set(PF_VERSION "${PROJECT_VERSION}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Generated")

# Fresh version stamp (0.1.0-beta.N · 12h time · git hash) on every build for testers.
set(PF_BUILDINFO_CPP "${CMAKE_BINARY_DIR}/Generated/BuildInfo.cpp")
set(PF_BUILDINFO_H   "${CMAKE_BINARY_DIR}/Generated/BuildInfo.h")
add_custom_target(MidiBrowserBuildInfo ALL
    COMMAND "${CMAKE_COMMAND}"
        -DPF_VERSION=${PF_VERSION}
        -DSRC_DIR=${CMAKE_SOURCE_DIR}
        -DOUT_DIR=${CMAKE_BINARY_DIR}/Generated
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake"
    BYPRODUCTS "${PF_BUILDINFO_CPP}" "${PF_BUILDINFO_H}"
    COMMENT "Updating BuildInfo stamp"
    VERBATIM)
# Seed so the first configure can resolve the source path before the first build.
# Skip incrementing the build counter on configure — only count real builds.
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DPF_VERSION=${PF_VERSION}
        -DSRC_DIR=${CMAKE_SOURCE_DIR}
        -DOUT_DIR=${CMAKE_BINARY_DIR}/Generated
        -DPF_SEED_ONLY=1
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake")

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
set(MIDIBROWSER_PRODUCT_NAME "Midi Toolkit")
juce_add_plugin(MidiBrowser
    COMPANY_NAME            "MidiBrowser"
    BUNDLE_ID               com.midibrowser.app
    PLUGIN_MANUFACTURER_CODE MdbR
    PLUGIN_CODE              MdbB
    FORMATS                  ${_midibrowser_formats}
    PRODUCT_NAME             "${MIDIBROWSER_PRODUCT_NAME}"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  TRUE
)

# Always install into the user plug-in folders so DAWs see the latest build.
# (JUCE's COPY_PLUGIN_AFTER_BUILD can be left OFF in an old CMake cache.)
# Quote each -D so paths with spaces (e.g. "Midi Toolkit.vst3") stay intact.
if(APPLE)
    foreach(_fmt IN ITEMS VST3 AU)
        if(TARGET MidiBrowser_${_fmt})
            if(_fmt STREQUAL "VST3")
                set(_kind VST3)
            else()
                set(_kind AU)
            endif()
            add_custom_command(TARGET MidiBrowser_${_fmt} POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    "-DPLUGIN_SRC=$<TARGET_BUNDLE_DIR:MidiBrowser_${_fmt}>"
                    "-DPLUGIN_KIND=${_kind}"
                    "-DPRODUCT_NAME=${MIDIBROWSER_PRODUCT_NAME}"
                    -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
                COMMENT "Installing ${_fmt} -> ~/Library/Audio/Plug-Ins"
                VERBATIM)
        endif()
    endforeach()
elseif(WIN32)
    if(TARGET MidiBrowser_VST3)
        add_custom_command(TARGET MidiBrowser_VST3 POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DPLUGIN_SRC=$<TARGET_FILE_DIR:MidiBrowser_VST3>/../.."
                "-DPLUGIN_KIND=VST3"
                "-DPRODUCT_NAME=${MIDIBROWSER_PRODUCT_NAME}"
                -P "${CMAKE_SOURCE_DIR}/cmake/InstallPlugins.cmake"
            COMMENT "Installing VST3 into Common Files"
            VERBATIM)
    endif()
endif()

juce_add_binary_data(MidiBrowserAssets
    SOURCES
        Resources/Fonts/SchibstedGrotesk.ttf
        Resources/Fonts/JetBrainsMono-Regular.ttf
        Resources/Fonts/JetBrainsMono-SemiBold.ttf
)

target_include_directories(MidiBrowser PRIVATE
    "${CMAKE_BINARY_DIR}/Generated"
    "${CMAKE_SOURCE_DIR}/Source")

target_sources(MidiBrowser PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MidiFileData.cpp
    Source/EditModel.cpp
    Source/GrooveEngine.cpp
    Source/LiveMidiFx.cpp
    Source/UiAtoms.cpp
    Source/TransportBar.cpp
    Source/BrowserPanels.cpp
    Source/KnobPanel.cpp
    Source/EffectsInspector.cpp
    Source/PianoRollEditor.cpp
    Source/Theme.cpp
    Source/DesignSystem.cpp
    Source/LibraryStore.cpp
    Source/FolderIndex.cpp
    "${PF_BUILDINFO_CPP}"
)
if(APPLE)
    target_sources(MidiBrowser PRIVATE Source/NativeWindowChrome.mm)
endif()
add_dependencies(MidiBrowser MidiBrowserBuildInfo)

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
        Tests/EditorUiTests.cpp
        Tests/LibraryStoreTests.cpp
        Tests/LiveMidiFxTests.cpp
        Tests/FolderIndexTests.cpp
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

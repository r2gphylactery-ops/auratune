cmake_minimum_required(VERSION 3.22)
project(AuraTune VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # no VC++ redist needed

# JUCE: use a local copy if you pass -DJUCE_DIR=..., otherwise download it automatically
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(AuraTune
    COMPANY_NAME              "Mikhail"
    PLUGIN_MANUFACTURER_CODE  Mkhl
    PLUGIN_CODE               Aura
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "Aura Tune"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES           Fx Pitch Shift
    COPY_PLUGIN_AFTER_BUILD   FALSE)

target_sources(AuraTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(AuraTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    _USE_MATH_DEFINES)

target_link_libraries(AuraTune
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional dev tool (not needed for the plugin): renders the UI to a PNG
option(AURA_BUILD_SNAPSHOT "Build UI snapshot tool" OFF)
if(AURA_BUILD_SNAPSHOT)
    juce_add_console_app(AuraSnapshot PRODUCT_NAME "AuraSnapshot")
    target_sources(AuraSnapshot PRIVATE tools/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(AuraSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Aura Tune"
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(AuraSnapshot PRIVATE juce::juce_audio_processors juce::juce_gui_basics
        PUBLIC juce::juce_recommended_config_flags)
endif()

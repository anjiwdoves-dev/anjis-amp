cmake_minimum_required(VERSION 3.22)
project(AnjisAmp VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(ANJI_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND ANJI_FORMATS AU)
endif()

set(ANJI_VST3_MANIFEST TRUE)
if(CMAKE_CROSSCOMPILING)
    set(ANJI_VST3_MANIFEST FALSE)   # can't run the Windows manifest helper while cross-compiling
endif()

juce_add_plugin(AnjisAmp
    VST3_AUTO_MANIFEST ${ANJI_VST3_MANIFEST}
    COMPANY_NAME "Anji"
    BUNDLE_ID "com.anji.anjisamp"
    PLUGIN_MANUFACTURER_CODE Anji
    PLUGIN_CODE AnAp
    PRODUCT_NAME "Anjis Amp"
    PLUGIN_NAME "Anji's Amp"
    DESCRIPTION "Guitar amp simulator"
    FORMATS ${ANJI_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Anji's Amp needs your audio interface input to process your guitar.")

target_sources(AnjisAmp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(AnjisAmp PRIVATE Source)

target_compile_definitions(AnjisAmp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AnjisAmp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional offline test (renders presets + editor snapshot). Not needed to build the plugin.
option(ANJI_BUILD_TESTS "Build offline test app" OFF)
if(ANJI_BUILD_TESTS)
    juce_add_console_app(AnjiTest PRODUCT_NAME "AnjiTest")
    target_sources(AnjiTest PRIVATE tools/plugin_test/main.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(AnjiTest PRIVATE Source)
    target_compile_definitions(AnjiTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Anji's Amp" JucePlugin_Build_Standalone=0)
    target_link_libraries(AnjiTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

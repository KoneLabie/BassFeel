cmake_minimum_required(VERSION 3.22)
project(BassFeel VERSION 0.1.0)

# Static runtime: users do NOT need to install the Visual C++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(BF_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND BF_FORMATS AU)
endif()

juce_add_plugin(BassFeel
    COMPANY_NAME "Kone Labie"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Ynam
    PLUGIN_CODE Bsfl
    FORMATS ${BF_FORMATS}
    VST3_CATEGORIES Fx EQ
    COPY_PLUGIN_AFTER_BUILD TRUE
    PRODUCT_NAME "BassFeel")

juce_generate_juce_header(BassFeel)

target_sources(BassFeel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/UI.h)

target_compile_definitions(BassFeel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BassFeel
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

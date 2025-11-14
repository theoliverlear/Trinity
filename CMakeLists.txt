cmake_minimum_required(VERSION 3.22)

project(Trinity VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(Trinity
        COMPANY_NAME "Sigwarth Software"
        BUNDLE_ID com.sigwarth.trinity
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        PLUGIN_MANUFACTURER_CODE Sigw
        PLUGIN_CODE Trin
        FORMATS VST3 Standalone
        PRODUCT_NAME "Trinity"
        COPY_PLUGIN_AFTER_BUILD TRUE
        VST3_COPY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/vst3"
)

juce_generate_juce_header(Trinity)

target_sources(Trinity PRIVATE
        source/TrinityProcessor.cpp
        source/TrinityProcessor.h
        source/TrinityEditor.cpp
        source/TrinityEditor.h
)

target_compile_definitions(Trinity PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Trinity PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
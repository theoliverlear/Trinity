cmake_minimum_required(VERSION 3.22)
include(FetchContent)

# Ensure MSVC uses the DLL runtime (MD/MDd) consistently across all targets, including dependencies
if (MSVC)
    # Applies to targets that honour this variable (CMake >= 3.15)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# GoogleTest (used for unit tests)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.0
)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
FetchContent_MakeAvailable(spdlog)

# As a safety net, also force the MSVC runtime on gtest targets (in case toolchain defaults differ)
if (MSVC)
    foreach(gtest_tgt IN ITEMS gtest gtest_main)
        if (TARGET ${gtest_tgt})
            # Ensure /MD or /MDd
            set_property(TARGET ${gtest_tgt} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
            # Align iterator debugging level in Debug to avoid LNK2038 with JUCE-built objects
            target_compile_definitions(${gtest_tgt} PRIVATE $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=2>)
        endif()
    endforeach()
endif()

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
        source/components/AudioMeter.cpp
        source/components/AudioMeter.h
        source/components/AudioSpectrumMeters.cpp
        source/components/AudioSpectrumMeters.h
        source/components/GraphicalSpectrumAnalyzer.cpp
        source/components/GraphicalSpectrumAnalyzer.h
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
        spdlog::spdlog
)

# Enable and add tests
include(CTest)
enable_testing()
add_subdirectory(test)
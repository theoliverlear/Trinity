cmake_minimum_required(VERSION 3.22)

set(TEST_SOURCES
        trinity_tests.cpp
        ../source/TrinityProcessor.cpp
        ../source/TrinityProcessor.h
        ../source/TrinityEditor.cpp
        ../source/TrinityEditor.h
        ../source/components/AudioMeter.cpp
        ../source/components/AudioMeter.h
        ../source/components/AudioSpectrumMeters.cpp
        ../source/components/AudioSpectrumMeters.h
        ../source/components/GraphicalSpectrumAnalyzer.cpp
        ../source/components/GraphicalSpectrumAnalyzer.h
)

juce_add_console_app(trinity_tests PRODUCT_NAME "trinity_tests")

juce_generate_juce_header(trinity_tests)

target_sources(trinity_tests PRIVATE
        ${TEST_SOURCES}
        # Compile GoogleTest directly into the test binary to ensure
        # it uses the exact same MSVC runtime and debug iterator level
        # as the rest of the project, avoiding cached/prebuilt lib mismatches.
        ${googletest_SOURCE_DIR}/googletest/src/gtest-all.cc
        ${googletest_SOURCE_DIR}/googletest/src/gtest_main.cc
)

# Include paths for the embedded GoogleTest sources
target_include_directories(trinity_tests PRIVATE
        ${googletest_SOURCE_DIR}/googletest/include
        ${googletest_SOURCE_DIR}/googletest
)

target_link_libraries(trinity_tests
        PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Ensure Debug builds match JUCE default iterator debugging level
target_compile_definitions(trinity_tests PRIVATE $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=2>)

target_compile_features(trinity_tests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(trinity_tests)

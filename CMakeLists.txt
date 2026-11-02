cmake_minimum_required(VERSION 3.22)

project(AutoThrow VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(AutoThrow
    COMPANY_NAME "Astral Vega Audio"
    BUNDLE_ID com.astralvega.autothrow
    PLUGIN_MANUFACTURER_CODE Asvg
    PLUGIN_CODE Athr
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Reverb
    PRODUCT_NAME "Auto Throw")

juce_generate_juce_header(AutoThrow)

# The DSP core has no plugin or GUI dependencies, so the offline tests can
# compile it directly.
set(AUTOTHROW_DSP_SOURCES
    Source/PhraseDetector.cpp
    Source/ThrowBus.cpp
    Source/AutoThrowCore.cpp)

set(AUTOTHROW_PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ThrowDisplay.cpp
    Source/ThrowLookAndFeel.cpp)

target_sources(AutoThrow PRIVATE
    ${AUTOTHROW_DSP_SOURCES}
    ${AUTOTHROW_PLUGIN_SOURCES})

target_compile_definitions(AutoThrow PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AutoThrow
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP tests: renders synthetic vocals through the core, checks
# detection and echo timing, and writes listenable WAVs to test-output/.
juce_add_console_app(AutoThrowTests PRODUCT_NAME "AutoThrowTests")

juce_generate_juce_header(AutoThrowTests)

target_sources(AutoThrowTests PRIVATE
    ${AUTOTHROW_DSP_SOURCES}
    Tests/AutoThrowTests.cpp)

target_include_directories(AutoThrowTests PRIVATE Source)

target_compile_definitions(AutoThrowTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(AutoThrowTests
    PRIVATE
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Plays a WAV through the processor and saves the editor as a PNG, so the
# display can be checked without a DAW.
juce_add_console_app(AutoThrowSnapshot PRODUCT_NAME "AutoThrowSnapshot")

juce_generate_juce_header(AutoThrowSnapshot)

target_sources(AutoThrowSnapshot PRIVATE
    ${AUTOTHROW_DSP_SOURCES}
    ${AUTOTHROW_PLUGIN_SOURCES}
    Tests/EditorSnapshot.cpp)

target_include_directories(AutoThrowSnapshot PRIVATE Source)

target_compile_definitions(AutoThrowSnapshot PRIVATE
    "JucePlugin_Name=\"Auto Throw\""
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(AutoThrowSnapshot
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

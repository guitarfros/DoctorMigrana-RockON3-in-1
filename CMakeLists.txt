cmake_minimum_required(VERSION 3.22)
project(DoctorMigrana VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded at configure time (needs internet the first time).
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

option(DM_COPY_AFTER_BUILD "Copy the plugin to the system VST3 folder after build" ON)

juce_add_plugin(DoctorMigrana
  COMPANY_NAME "GuitarFros"
  BUNDLE_ID com.guitarfros.doctormigrana.rockon
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  PLUGIN_MANUFACTURER_CODE Dmgr
  PLUGIN_CODE Dmig
  FORMATS VST3 Standalone
  VST3_CATEGORIES Fx Distortion
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Doctor Migrana – RockON 3-in-1 needs audio input to process your guitar."
  PLUGIN_NAME "Doctor Migrana – RockON 3-in-1"        # name shown by the DAW (note: en dash, UTF-8)
  PRODUCT_NAME "doctormigranaRockOn3-1"                # file name: doctormigranaRockOn3-1.vst3 / .so / standalone
  COPY_PLUGIN_AFTER_BUILD ${DM_COPY_AFTER_BUILD})   # Linux: ~/.vst3

juce_add_binary_data(DMAssets SOURCES
  assets/bg.jpg
  assets/lever_on.png
  assets/lever_off.png
  assets/mode_up.png
  assets/mode_down.png
  assets/series_up.png
  assets/series_down.png)
set_target_properties(DMAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(DoctorMigrana PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

# The plugin name contains a non-ASCII character (en dash): make sure MSVC reads sources/definitions as UTF-8.
if(MSVC)
  target_compile_options(DoctorMigrana PRIVATE /utf-8)
endif()

target_compile_definitions(DoctorMigrana PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DoctorMigrana PRIVATE
  DMAssets
  juce::juce_audio_utils
  juce::juce_dsp
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)

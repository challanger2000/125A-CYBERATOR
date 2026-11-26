cmake_minimum_required(VERSION 3.24)
project(CyberatorLocalProbe LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Local probe requires Windows x64")
endif()
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 654bdbc7a9db0dae90849b44abe91f339e9f0768
    GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(JUCE)
add_executable(vst3_host_test ../../tests/vst3_host_test.cpp)
target_compile_features(vst3_host_test PRIVATE cxx_std_23)
target_include_directories(vst3_host_test PRIVATE ../../src)
target_compile_definitions(vst3_host_test PRIVATE CYBERATOR_ASIO_PROBE=1 JUCE_ASIO=1 NOMINMAX JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(vst3_host_test PRIVATE juce::juce_audio_processors juce::juce_audio_devices juce::juce_recommended_config_flags)

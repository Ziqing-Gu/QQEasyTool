cmake_minimum_required(VERSION 3.22)

project(QQEasyTool VERSION 1.0.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(QQDEBREATH_COPY_AFTER_BUILD "Copy built VST3 to the user common VST3 folder" ON)
option(QQDEBREATH_FETCH_JUCE "Download JUCE with CMake FetchContent" ON)
set(QQDEBREATH_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout when QQDEBREATH_FETCH_JUCE is OFF")
set(QQDEBREATH_ARA_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/ARA_SDK" CACHE PATH "Path to the ARA SDK umbrella directory")
set(QQDEBREATH_TOOL_EXE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../dist/QQDeBreathTool/QQDeBreathTool.exe" CACHE FILEPATH "Optional QQDeBreathTool exe for bridge debugging. ARA 1.03 uses native C++ analysis by default.")
set(QQDEBREATH_BRIDGE_EXE_PATH "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/qq_debreath_bridge.exe")

set(QQDEBREATH_JUCE_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE")
if(EXISTS "${QQDEBREATH_JUCE_LOCAL_DIR}/CMakeLists.txt")
    add_subdirectory("${QQDEBREATH_JUCE_LOCAL_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
elseif(QQDEBREATH_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.13
    )
    FetchContent_MakeAvailable(JUCE)
else()
    if(NOT QQDEBREATH_JUCE_PATH)
        message(FATAL_ERROR "Set QQDEBREATH_JUCE_PATH to a local JUCE checkout, or enable QQDEBREATH_FETCH_JUCE.")
    endif()
    add_subdirectory("${QQDEBREATH_JUCE_PATH}" JUCE)
endif()

if(EXISTS "${QQDEBREATH_ARA_SDK_PATH}/ARA_API/ARAVST3.h")
    juce_set_ara_sdk_path("${QQDEBREATH_ARA_SDK_PATH}")
else()
    message(WARNING "ARA SDK not found at QQDEBREATH_ARA_SDK_PATH='${QQDEBREATH_ARA_SDK_PATH}'. Unified QQDeBreath ARA support requires the ARA SDK.")
endif()

set(QQDEBREATH_PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND QQDEBREATH_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(QQEasyTool
    COMPANY_NAME "Qing Audio"
    PRODUCT_NAME "QQEasyTool"
    BUNDLE_ID "com.qqeasytool.QQEasyTool"
    PLUGIN_MANUFACTURER_CODE Qqet
    PLUGIN_CODE Qe09
    FORMATS ${QQDEBREATH_PLUGIN_FORMATS}
    IS_ARA_EFFECT TRUE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${QQDEBREATH_COPY_AFTER_BUILD}
    VST3_CATEGORIES Fx
)

target_sources(QQEasyTool
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/Parameters.cpp
        src/shared/BridgeAnalysis.cpp
        src/shared/BreathEq.cpp
        src/shared/BreathEqComponent.cpp
        src/shared/NativeAnalysis.cpp
        src/shared/WaveformEditorComponent.cpp
)

target_include_directories(QQEasyTool
    PRIVATE
        src
)

target_compile_features(QQEasyTool PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(QQEasyTool PRIVATE /utf-8)
endif()

target_compile_definitions(QQEasyTool
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        QQDEBREATH_BRIDGE_EXE_PATH="${QQDEBREATH_BRIDGE_EXE_PATH}"
        QQDEBREATH_TOOL_EXE_PATH="${QQDEBREATH_TOOL_EXE_PATH}"
)

target_link_libraries(QQEasyTool
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

add_executable(qq_debreath_bridge
    bridge/src/main.cpp
)

target_compile_features(qq_debreath_bridge PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(qq_debreath_bridge PRIVATE /utf-8)
    target_link_options(qq_debreath_bridge PRIVATE /ENTRY:wmainCRTStartup)
endif()

target_compile_definitions(qq_debreath_bridge
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(qq_debreath_bridge
    PRIVATE
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_executable(qq_debreath_native_probe
    native_probe/src/main.cpp
    src/shared/NativeAnalysis.cpp
)

target_include_directories(qq_debreath_native_probe
    PRIVATE
        src
)

target_compile_features(qq_debreath_native_probe PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(qq_debreath_native_probe PRIVATE /utf-8)
    target_link_options(qq_debreath_native_probe PRIVATE /ENTRY:wmainCRTStartup)
endif()

target_compile_definitions(qq_debreath_native_probe
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(qq_debreath_native_probe
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
option(QQEASYTOOL_BUILD_ROUTE_PROBE "Build deterministic audio-route probe" ON)
if(QQEASYTOOL_BUILD_ROUTE_PROBE)
    add_executable(qq_easytool_route_probe
        tests/AudioRouteProbe.cpp
        "${JUCE_MODULES_DIR}/juce_audio_plugin_client/juce_audio_plugin_client_ARA.cpp"
    )

    target_include_directories(qq_easytool_route_probe
        PRIVATE
            src
    )

    target_compile_features(qq_easytool_route_probe PRIVATE cxx_std_17)
    target_compile_definitions(qq_easytool_route_probe
        PRIVATE
            QQEASYTOOL_ENABLE_ROUTE_TESTS=1
            JUCE_WEB_BROWSER=0
            JucePlugin_Build_VST3=1
            JUCE_USE_CURL=0
    )

    if(MSVC)
        target_compile_options(qq_easytool_route_probe PRIVATE /utf-8)
    endif()

    target_link_libraries(qq_easytool_route_probe
        PRIVATE
            QQEasyTool
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endif()

option(QQEASYTOOL_BUILD_TRANSPORT_SYNC_PROBE "Build deterministic host transport sync probe" ON)
if(QQEASYTOOL_BUILD_TRANSPORT_SYNC_PROBE)
    add_executable(qq_easytool_transport_sync_probe
        tests/TransportSyncProbe.cpp
        "${JUCE_MODULES_DIR}/juce_audio_plugin_client/juce_audio_plugin_client_ARA.cpp"
    )
    target_include_directories(qq_easytool_transport_sync_probe PRIVATE src)
    target_compile_features(qq_easytool_transport_sync_probe PRIVATE cxx_std_17)
    target_compile_definitions(qq_easytool_transport_sync_probe PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Build_VST3=1
    )
    if(MSVC)
        target_compile_options(qq_easytool_transport_sync_probe PRIVATE /utf-8)
        target_link_options(qq_easytool_transport_sync_probe PRIVATE /ENTRY:wmainCRTStartup)
    endif()
    target_link_libraries(qq_easytool_transport_sync_probe
        PRIVATE QQEasyTool juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags
    )
endif()

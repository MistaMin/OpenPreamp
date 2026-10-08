cmake_minimum_required(VERSION 3.22)

# Single source of truth for the plugin version: VERSION (MAJOR.MINOR.PATCH).
# Bump that file (or answer the installer prompt) - nothing else hardcodes it.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HYBRIDEQ_VERSION_STR)
string(STRIP "${HYBRIDEQ_VERSION_STR}" HYBRIDEQ_VERSION_STR)
project(HybridEQ VERSION ${HYBRIDEQ_VERSION_STR} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
)
FetchContent_MakeAvailable(JUCE)

set(GOODLOOKINUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/GoodLookinUI" CACHE PATH "GoodLookinUI toolkit location")
add_subdirectory("${GOODLOOKINUI_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/goodlookinui")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Designs/HybridEQ.csv")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Designs/HybridEQ.csv" HYBRIDEQ_DESIGN_TEXT)
configure_file(Designs/EmbeddedDesign.h.in generated/EmbeddedDesign.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Designs/LookTriggers.csv")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Designs/LookTriggers.csv" HYBRIDEQ_LOOKS_TEXT)
configure_file(Designs/EmbeddedLooks.h.in generated/EmbeddedLooks.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Designs/Settings.csv")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Designs/Settings.csv" HYBRIDEQ_SETTINGS_TEXT)
configure_file(Designs/EmbeddedSettings.h.in generated/EmbeddedSettings.h @ONLY)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Designs/OpenPreampKnobs.csv" OPENPREAMP_KNOB_DESIGN)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Designs/OpenPreampLooks.csv" OPENPREAMP_MODEL_LOOKS)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Designs/OpenPreampKnobs.csv" "${CMAKE_CURRENT_SOURCE_DIR}/Designs/OpenPreampLooks.csv")
configure_file(Designs/EmbeddedOpenPreampDesign.h.in generated/EmbeddedOpenPreampDesign.h @ONLY)
option(HYBRIDEQ_DESIGN_EDITOR "Include GoodLookinUI editor in Debug builds" ON)

set(HYBRIDEQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND HYBRIDEQ_FORMATS AU)
endif()
# Open formats (also the ones used on Linux): LV2 through JUCE, CLAP through clap-juce-extensions (below).
option(HYBRIDEQ_LV2 "Build the LV2 format" ON)
option(HYBRIDEQ_CLAP "Build the CLAP format (fetches clap-juce-extensions)" ON)
if(HYBRIDEQ_LV2)
    list(APPEND HYBRIDEQ_FORMATS LV2)
endif()
set(HYBRIDEQ_AAX_SDK "${CMAKE_SOURCE_DIR}/../aax-sdk-2-8-1" CACHE PATH "Optional Avid AAX SDK")
if(EXISTS "${HYBRIDEQ_AAX_SDK}/Interfaces/AAX.h")
    juce_set_aax_sdk_path("${HYBRIDEQ_AAX_SDK}")
    list(APPEND HYBRIDEQ_FORMATS AAX)
else()
    message(STATUS "AAX SDK not found; building ${HYBRIDEQ_FORMATS}")
endif()

juce_add_plugin(HybridEQ
    COMPANY_NAME            "OpenGrid"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    PLUGIN_MANUFACTURER_CODE Hybr
    PLUGIN_CODE              HyEq
    FORMATS                  ${HYBRIDEQ_FORMATS}
    LV2URI                   "https://github.com/MistaMin/HybridEQ"
    PRODUCT_NAME             "HybridEQ"
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

if(HYBRIDEQ_CLAP)
    FetchContent_Declare(
        clap_juce_extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG        7adee3a1bd4684d4caa5601100e364abccff4b4d
    )
    FetchContent_MakeAvailable(clap_juce_extensions)
    clap_juce_extensions_plugin(
        TARGET HybridEQ
        CLAP_ID "com.opengrid.hybrideq"
        CLAP_FEATURES audio-effect equalizer stereo
        CLAP_MANUAL_URL "https://github.com/MistaMin/HybridEQ"
        CLAP_SUPPORT_URL "https://github.com/MistaMin/HybridEQ")
    list(APPEND HYBRIDEQ_FORMATS CLAP)
endif()

# ---------------------------------------------------------------------------
# OpenPreamp: standalone preamp-only plugin (no EQ, no harmonics panel).
# It reuses the DSP classes but targets a separate plugin binary with its own
# parameter layout and a simplified editor.
# ---------------------------------------------------------------------------
set(OPENPREAMP_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND OPENPREAMP_FORMATS AU)
endif()
if(HYBRIDEQ_LV2)
    list(APPEND OPENPREAMP_FORMATS LV2)
endif()
if(EXISTS "${HYBRIDEQ_AAX_SDK}/Interfaces/AAX.h")
    list(APPEND OPENPREAMP_FORMATS AAX) # Private builds only; public packaging excludes AAX.
endif()

juce_add_plugin(OpenPreamp
    COMPANY_NAME            "OpenGrid"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    PLUGIN_MANUFACTURER_CODE OPre
    PLUGIN_CODE              OpPr
    FORMATS                  ${OPENPREAMP_FORMATS}
    LV2URI                   "https://github.com/MistaMin/OpenPreamp"
    PRODUCT_NAME             "OpenPreamp"
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

if(HYBRIDEQ_CLAP)
    clap_juce_extensions_plugin(
        TARGET OpenPreamp
        CLAP_ID "com.opengrid.openpreamp"
        CLAP_FEATURES audio-effect preamp distortion
        CLAP_MANUAL_URL "https://github.com/MistaMin/OpenPreamp"
        CLAP_SUPPORT_URL "https://github.com/MistaMin/OpenPreamp")
    list(APPEND OPENPREAMP_FORMATS CLAP)
endif()

target_sources(OpenPreamp PRIVATE
    Source/OpenPreampPlugin/PluginProcessor.cpp
    Source/OpenPreampPlugin/PluginEditor.cpp
)

target_include_directories(OpenPreamp PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/generated")

option(OPENPREAMP_DEVELOPER_MODE "Enable OpenPreamp knob/layout and model-look editors" ON)
target_compile_definitions(OpenPreamp PUBLIC
    GOODLOOKINUI_ENABLE_EDITOR=$<BOOL:${OPENPREAMP_DEVELOPER_MODE}>
    OPENPREAMP_DESIGNS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Designs"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(OpenPreamp
    PRIVATE
        goodlookinui_juce
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

foreach(format IN LISTS OPENPREAMP_FORMATS)
    if(format STREQUAL "LV2")
        set(notice_dir "$<TARGET_FILE_DIR:OpenPreamp_${format}>/Licenses")
    elseif(APPLE)
        set(notice_dir "$<TARGET_FILE_DIR:OpenPreamp_${format}>/../Resources/Licenses")
    else()
        set(notice_dir "$<TARGET_FILE_DIR:OpenPreamp_${format}>/OpenPreamp-Licenses")
    endif()
    add_custom_command(TARGET OpenPreamp_${format} PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E make_directory "${notice_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            "${notice_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Licenses/OpenPreamp-BINARY_LICENSE.txt" "${notice_dir}/BINARY_LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Licenses/OpenPreamp-THIRD_PARTY_NOTICES.txt" "${notice_dir}/THIRD_PARTY_NOTICES.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/GoodLookinUI/LICENSE"
            "${notice_dir}/GoodLookinUI-LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${notice_dir}/third-party"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Licenses/third-party"
            "${notice_dir}/third-party"
        VERBATIM)
endforeach()

option(HYBRIDEQ_BUILD_TESTS "Build the processor stress test" ON)
if(HYBRIDEQ_BUILD_TESTS)
    juce_add_console_app(OpenPreampADAA PRODUCT_NAME "OpenPreamp ADAA Test")
    target_sources(OpenPreampADAA PRIVATE tests/openpreamp_adaa.cpp)
    target_include_directories(OpenPreampADAA PRIVATE Source)
    target_compile_definitions(OpenPreampADAA PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(OpenPreampADAA PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)
    juce_add_console_app(OpenPreampSmoke PRODUCT_NAME "OpenPreamp Smoke Test")
    target_sources(OpenPreampSmoke PRIVATE tests/openpreamp_smoke.cpp)
    target_include_directories(OpenPreampSmoke PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(OpenPreampSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(OpenPreampSmoke PRIVATE OpenPreamp goodlookinui_juce juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    juce_add_console_app(OpenPreampCPU PRODUCT_NAME "OpenPreamp CPU Table")
    target_sources(OpenPreampCPU PRIVATE tests/openpreamp_cpu_table.cpp)
    target_include_directories(OpenPreampCPU PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(OpenPreampCPU PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(OpenPreampCPU PRIVATE OpenPreamp goodlookinui_juce juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

target_sources(HybridEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(HybridEQ PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_compile_definitions(HybridEQ PUBLIC
    GOODLOOKINUI_ENABLE_EDITOR=$<AND:$<CONFIG:Debug>,$<BOOL:${HYBRIDEQ_DESIGN_EDITOR}>>
    # Developer builds autosave every design change into this folder (Designs/*.csv) and start from it
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${HYBRIDEQ_DESIGN_EDITOR}>>:HYBRIDEQ_DESIGNS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Designs">
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(HybridEQ
    PRIVATE
        goodlookinui_juce
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Keep source/component MIT notices with standalone and raw plugin downloads.
foreach(format IN LISTS HYBRIDEQ_FORMATS)
    if(format STREQUAL "LV2")
        set(notice_dir "$<TARGET_FILE_DIR:HybridEQ_${format}>/Licenses")
    elseif(APPLE)
        set(notice_dir "$<TARGET_FILE_DIR:HybridEQ_${format}>/../Resources/Licenses")
    else()
        set(notice_dir "$<TARGET_FILE_DIR:HybridEQ_${format}>/HybridEQ-Licenses")
    endif()
    add_custom_command(TARGET HybridEQ_${format} PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E make_directory "${notice_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/BINARY_LICENSE.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt"
            "${notice_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/GoodLookinUI/LICENSE"
            "${notice_dir}/GoodLookinUI-LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${notice_dir}/third-party"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Licenses/third-party"
            "${notice_dir}/third-party"
        VERBATIM)
endforeach()

# Headless stress test of the real processor (rates, block sizes, oversampling, automation, threads).
option(HYBRIDEQ_BUILD_TESTS "Build the processor stress test" ON)
if(HYBRIDEQ_BUILD_TESTS)
    juce_add_console_app(HybridEQStress PRODUCT_NAME "HybridEQ Stress Test")
    target_sources(HybridEQStress PRIVATE tests/processor_stress.cpp)
    target_include_directories(HybridEQStress PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(HybridEQStress PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 GOODLOOKINUI_ENABLE_EDITOR=0)
    target_link_libraries(HybridEQStress PRIVATE HybridEQ goodlookinui_juce juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

# Per-configuration CPU table for the preamp circuit path (oversampling x rate x block size).
if(HYBRIDEQ_BUILD_TESTS)
    juce_add_console_app(HybridEQCPU PRODUCT_NAME "HybridEQ CPU Table")
    target_sources(HybridEQCPU PRIVATE tests/cpu_table.cpp)
    target_include_directories(HybridEQCPU PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(HybridEQCPU PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 GOODLOOKINUI_ENABLE_EDITOR=0)
    target_link_libraries(HybridEQCPU PRIVATE HybridEQ goodlookinui_juce juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

# Developer-build autosave test: drives the real editor and checks that design changes reach Designs/*.csv
# (in a scratch folder). Only meaningful in Debug builds, where the developer editor is compiled in.
if(HYBRIDEQ_BUILD_TESTS AND HYBRIDEQ_DESIGN_EDITOR)
    juce_add_console_app(HybridEQAutosaveTest PRODUCT_NAME "HybridEQ Autosave Test")
    target_sources(HybridEQAutosaveTest PRIVATE tests/autosave_editor.cpp)
    target_include_directories(HybridEQAutosaveTest PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(HybridEQAutosaveTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(HybridEQAutosaveTest PRIVATE HybridEQ goodlookinui_juce juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

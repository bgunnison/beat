# Minimal CMake scaffolding for the Beat VST3 MIDI-only plugin.
# Point VST3_SDK_ROOT to your local Steinberg VST3 SDK checkout.
cmake_minimum_required(VERSION 3.20)
project(BeatVST3 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{VST3_SDK_ROOT} AND NOT VST3_SDK_ROOT)
  message(FATAL_ERROR "Set VST3_SDK_ROOT to the Steinberg VST3 SDK path.")
endif()

if(NOT VST3_SDK_ROOT)
  set(VST3_SDK_ROOT $ENV{VST3_SDK_ROOT})
endif()

# Normalize path separators for CMake.
file(TO_CMAKE_PATH "${VST3_SDK_ROOT}" VST3_SDK_ROOT)

list(APPEND CMAKE_MODULE_PATH "${VST3_SDK_ROOT}/cmake/modules")
include("${VST3_SDK_ROOT}/cmake/modules/SMTG_VST3_SDK.cmake")
include("${VST3_SDK_ROOT}/cmake/modules/SMTG_AddVST3Library.cmake")

# Avoid building SDK samples/tools we don't need.
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_MODULE_INFO OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

# Bring in SDK targets (validator, moduleinfotool, etc.).
add_subdirectory(${VST3_SDK_ROOT} ${CMAKE_BINARY_DIR}/vst3sdk)

set(Beat_SOURCES
    src/BeatEngine.cpp
    src/BeatProcessor.cpp
    src/BeatController.cpp
    src/BeatPluginFactory.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/main/dllmain.cpp
)

set(Beat_HEADERS
    src/BeatEngine.h
    src/BeatProcessor.h
    src/BeatController.h
    src/BeatIDs.h
)

smtg_add_vst3plugin(Beat
    PACKAGE_NAME "Beat"
    SOURCES_LIST ${Beat_SOURCES} ${Beat_HEADERS}
)

target_include_directories(Beat PRIVATE
    ${VST3_SDK_ROOT}
)

target_link_libraries(Beat PRIVATE
    sdk
    sdk_common
    base
    pluginterfaces
    vstgui_support
)

target_compile_definitions(Beat PRIVATE
    $<$<CONFIG:Debug>:VSTGUI_LIVE_EDITING=1>
)

set(BEAT_UIDESC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/beat.uidesc.in)
set(BEAT_UIDESC_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/beat.uidesc)
set(BEAT_BUNDLE_UIDESC "C:/ProgramData/vstplugins/Beat.vst3/Contents/Resources/beat.uidesc" CACHE STRING "Path to installed Beat.vst3 UIDesc")
option(BEAT_IMPORT_UIDESC_ON_BUILD "Import edited UI from installed plugin bundle" ON)

add_custom_target(BeatUIDescImport
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_BUNDLE=${BEAT_BUNDLE_UIDESC}
        -DOUTPUT_TEMPLATE=${BEAT_UIDESC_TEMPLATE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ImportUIDesc.cmake
    VERBATIM
)

add_custom_target(BeatUIDesc ALL
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${BEAT_UIDESC_TEMPLATE}
        -DOUTPUT=${BEAT_UIDESC_GENERATED}
        -DPROJECT_VERSION=${PROJECT_VERSION}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateUIDesc.cmake
    BYPRODUCTS ${BEAT_UIDESC_GENERATED}
    VERBATIM
)

if(BEAT_IMPORT_UIDESC_ON_BUILD)
    add_dependencies(BeatUIDesc BeatUIDescImport)
endif()
add_dependencies(Beat BeatUIDesc)

set(Beat_RESOURCES
    ${BEAT_UIDESC_GENERATED}
)

smtg_target_add_plugin_resources(Beat
    RESOURCES
        ${Beat_RESOURCES}
)
